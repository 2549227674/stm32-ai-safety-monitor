// Persistent Qwen3-VL worker: loads models once, serves multiple requests via stdin/stdout.
// Protocol: one JSON line per request on stdin, one JSON line per response on stdout.
// Request:  {"image_path":"/tmp/x.jpg","question":"describe"}
// Response: {"ok":true,"text":"...","total_ms":N,"imgenc_ms":N,"llm_ms":N}
// Health:   {"action":"health"} -> {"ok":true,"model_loaded":true,...}
// Quit:     {"action":"quit"}

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <opencv2/opencv.hpp>
#include "image_enc.h"
#include "rkllm.h"

using namespace std;

static string g_output;

static int llm_callback(RKLLMResult *result, void *userdata, LLMCallState state) {
    if (state == RKLLM_RUN_FINISH) {
        // done
    } else if (state == RKLLM_RUN_ERROR) {
        g_output += "[ERROR]";
    } else if (state == RKLLM_RUN_NORMAL) {
        g_output += result->text;
    }
    return 0;
}

static cv::Mat expand2square(const cv::Mat& img, const cv::Scalar& bg) {
    int w = img.cols, h = img.rows;
    if (w == h) return img.clone();
    int sz = max(w, h);
    cv::Mat result(sz, sz, img.type(), bg);
    cv::Rect roi((sz - w) / 2, (sz - h) / 2, w, h);
    img.copyTo(result(roi));
    return result;
}

static string escape_json(const string& s) {
    string out;
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += c;
        }
    }
    return out;
}

// Minimal JSON value extraction (no external deps)
static string json_get_string(const string& json, const string& key) {
    string needle = "\"" + key + "\"";
    auto pos = json.find(needle);
    if (pos == string::npos) return "";
    pos = json.find(':', pos + needle.size());
    if (pos == string::npos) return "";
    pos++;
    while (pos < json.size() && json[pos] == ' ') pos++;
    if (pos >= json.size() || json[pos] != '"') return "";
    pos++;
    auto end = json.find('"', pos);
    if (end == string::npos) return "";
    return json.substr(pos, end - pos);
}

int main(int argc, char** argv) {
    const char* encoder_path = getenv("QWEN3VL_ENCODER");
    const char* llm_path = getenv("QWEN3VL_LLM");
    int core_num = 3;
    const char* core_str = getenv("QWEN3VL_CORE_NUM");
    if (core_str) core_num = atoi(core_str);

    if (!encoder_path || !llm_path) {
        printf("{\"ok\":false,\"error\":\"QWEN3VL_ENCODER and QWEN3VL_LLM env vars required\"}\n");
        fflush(stdout);
        return 1;
    }

    // Init LLM (once)
    RKLLMParam param = rkllm_createDefaultParam();
    param.model_path = llm_path;
    param.top_k = 1;
    param.max_new_tokens = 512;
    param.max_context_len = 4096;
    param.skip_special_token = true;
    param.extend_param.base_domain_id = 1;
    param.img_start = "<|vision_start|>";
    param.img_end = "<|vision_end|>";
    param.img_content = "<|image_pad|>";

    LLMHandle llmHandle = nullptr;
    int ret = rkllm_init(&llmHandle, &param, llm_callback);
    if (ret != 0) {
        printf("{\"ok\":false,\"error\":\"rkllm_init failed\"}\n");
        fflush(stdout);
        return 1;
    }

    // Init vision encoder (once)
    rknn_app_context_t rknn_app_ctx;
    memset(&rknn_app_ctx, 0, sizeof(rknn_app_context_t));
    ret = init_imgenc(encoder_path, &rknn_app_ctx, core_num);
    if (ret != 0) {
        printf("{\"ok\":false,\"error\":\"init_imgenc failed ret=%d\"}\n", ret);
        fflush(stdout);
        rkllm_destroy(llmHandle);
        return 1;
    }

    size_t n_image_tokens = rknn_app_ctx.model_image_token;
    size_t image_embed_len = rknn_app_ctx.model_embed_size;
    size_t n_embed_output = rknn_app_ctx.io_num.n_output;
    int rkllm_image_embed_len = n_image_tokens * image_embed_len * n_embed_output;

    // Signal ready
    printf("{\"ok\":true,\"action\":\"ready\",\"model_loaded\":true}\n");
    fflush(stdout);

    // Request loop
    string line;
    while (getline(cin, line)) {
        if (line.empty()) continue;

        // Parse action
        string action = json_get_string(line, "action");
        if (action == "quit") {
            printf("{\"ok\":true,\"action\":\"quit\"}\n");
            fflush(stdout);
            break;
        }
        if (action == "health") {
            printf("{\"ok\":true,\"service\":\"qwen3vl-worker\",\"model_loaded\":true,"
                   "\"model\":\"qwen3-vl-2b\",\"backend\":\"rknn-llm\","
                   "\"encoder\":\"%s\",\"llm\":\"%s\"}\n",
                   encoder_path, llm_path);
            fflush(stdout);
            continue;
        }

        // Infer request
        string image_path = json_get_string(line, "image_path");
        string question = json_get_string(line, "question");
        if (image_path.empty()) {
            printf("{\"ok\":false,\"error\":\"missing image_path\"}\n");
            fflush(stdout);
            continue;
        }
        if (question.empty()) {
            question = "请用一句话描述画面内容，并判断是否存在人员、烟雾、火焰或明显安全异常。";
        }

        auto t_total = chrono::high_resolution_clock::now();

        // Read and preprocess image
        cv::Mat img = cv::imread(image_path);
        if (img.empty()) {
            printf("{\"ok\":false,\"error\":\"cannot read image: %s\"}\n", image_path.c_str());
            fflush(stdout);
            continue;
        }
        cv::cvtColor(img, img, cv::COLOR_BGR2RGB);
        cv::Scalar bg(127.5, 127.5, 127.5);
        cv::Mat sq = expand2square(img, bg);
        cv::Mat resized;
        cv::resize(sq, resized,
                   cv::Size(rknn_app_ctx.model_width, rknn_app_ctx.model_height));

        // Encode image
        vector<float> img_vec(rkllm_image_embed_len, 0);
        auto t0 = chrono::high_resolution_clock::now();
        ret = run_imgenc(&rknn_app_ctx, resized.data, img_vec.data());
        if (ret != 0) {
            printf("{\"ok\":false,\"error\":\"run_imgenc failed ret=%d\"}\n", ret);
            fflush(stdout);
            continue;
        }
        auto t1 = chrono::high_resolution_clock::now();
        double imgenc_ms = chrono::duration_cast<chrono::microseconds>(t1 - t0).count() / 1000.0;

        // Run LLM inference
        g_output.clear();
        RKLLMInput rkllm_input;
        memset(&rkllm_input, 0, sizeof(RKLLMInput));
        RKLLMInferParam rkllm_infer_params;
        memset(&rkllm_infer_params, 0, sizeof(RKLLMInferParam));
        rkllm_infer_params.mode = RKLLM_INFER_GENERATE;
        rkllm_infer_params.keep_history = 0;

        string prompt = string("<image>") + question;
        rkllm_input.input_type = RKLLM_INPUT_MULTIMODAL;
        rkllm_input.role = "user";
        rkllm_input.multimodal_input.prompt = (char*)prompt.c_str();
        rkllm_input.multimodal_input.image_embed = img_vec.data();
        rkllm_input.multimodal_input.n_image_tokens = n_image_tokens;
        rkllm_input.multimodal_input.n_image = 1;
        rkllm_input.multimodal_input.image_height = rknn_app_ctx.model_height;
        rkllm_input.multimodal_input.image_width = rknn_app_ctx.model_width;

        t0 = chrono::high_resolution_clock::now();
        rkllm_run(llmHandle, &rkllm_input, &rkllm_infer_params, NULL);
        t1 = chrono::high_resolution_clock::now();
        double llm_ms = chrono::duration_cast<chrono::microseconds>(t1 - t0).count() / 1000.0;

        auto t_total_end = chrono::high_resolution_clock::now();
        double total_ms = chrono::duration_cast<chrono::microseconds>(t_total_end - t_total).count() / 1000.0;

        printf("{\"ok\":true,\"text\":\"%s\",\"total_ms\":%.0f,\"imgenc_ms\":%.0f,"
               "\"llm_ms\":%.0f}\n",
               escape_json(g_output).c_str(), total_ms, imgenc_ms, llm_ms);
        fflush(stdout);
    }

    release_imgenc(&rknn_app_ctx);
    rkllm_destroy(llmHandle);
    return 0;
}
