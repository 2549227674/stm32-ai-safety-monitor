// Single-shot Qwen3-VL inference wrapper for integration with Python Flask service
// Usage: ./qwen3vl_single_shot <image_path> <encoder_model> <llm_model> <question> <core_num>
// Output: JSON line with {"ok":true,"text":"...","latency_ms":N,"imgenc_ms":N,"llm_ms":N}

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

int callback(RKLLMResult *result, void *userdata, LLMCallState state) {
    if (state == RKLLM_RUN_FINISH) {
        // done
    } else if (state == RKLLM_RUN_ERROR) {
        g_output += "[ERROR]";
    } else if (state == RKLLM_RUN_NORMAL) {
        g_output += result->text;
    }
    return 0;
}

cv::Mat expand2square(const cv::Mat& img, const cv::Scalar& bg) {
    int w = img.cols, h = img.rows;
    if (w == h) return img.clone();
    int sz = max(w, h);
    cv::Mat result(sz, sz, img.type(), bg);
    cv::Rect roi((sz - w) / 2, (sz - h) / 2, w, h);
    img.copyTo(result(roi));
    return result;
}

int main(int argc, char** argv) {
    if (argc < 6) {
        cerr << "Usage: " << argv[0]
             << " image_path encoder_model llm_model question core_num" << endl;
        return 1;
    }

    const char* image_path = argv[1];
    const char* encoder_path = argv[2];
    const char* llm_path = argv[3];
    const char* question = argv[4];
    int core_num = atoi(argv[5]);

    auto t_total = chrono::high_resolution_clock::now();

    // Init LLM
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
    auto t0 = chrono::high_resolution_clock::now();
    int ret = rkllm_init(&llmHandle, &param, callback);
    if (ret != 0) {
        printf("{\"ok\":false,\"error\":\"rkllm_init failed\"}\n");
        return 1;
    }
    auto t1 = chrono::high_resolution_clock::now();
    double llm_load_ms = chrono::duration_cast<chrono::microseconds>(t1 - t0).count() / 1000.0;

    // Init vision encoder
    rknn_app_context_t rknn_app_ctx;
    memset(&rknn_app_ctx, 0, sizeof(rknn_app_context_t));
    t0 = chrono::high_resolution_clock::now();
    ret = init_imgenc(encoder_path, &rknn_app_ctx, core_num);
    if (ret != 0) {
        printf("{\"ok\":false,\"error\":\"init_imgenc failed ret=%d\"}\n", ret);
        rkllm_destroy(llmHandle);
        return 1;
    }
    t1 = chrono::high_resolution_clock::now();
    double enc_load_ms = chrono::duration_cast<chrono::microseconds>(t1 - t0).count() / 1000.0;

    // Read and preprocess image
    cv::Mat img = cv::imread(image_path);
    if (img.empty()) {
        printf("{\"ok\":false,\"error\":\"cannot read image\"}\n");
        release_imgenc(&rknn_app_ctx);
        rkllm_destroy(llmHandle);
        return 1;
    }
    cv::cvtColor(img, img, cv::COLOR_BGR2RGB);
    cv::Scalar bg(127.5, 127.5, 127.5);
    cv::Mat sq = expand2square(img, bg);
    cv::Mat resized;
    cv::resize(sq, resized,
               cv::Size(rknn_app_ctx.model_width, rknn_app_ctx.model_height));

    // Encode image
    size_t n_image_tokens = rknn_app_ctx.model_image_token;
    size_t image_embed_len = rknn_app_ctx.model_embed_size;
    size_t n_embed_output = rknn_app_ctx.io_num.n_output;
    int rkllm_image_embed_len = n_image_tokens * image_embed_len * n_embed_output;
    vector<float> img_vec(rkllm_image_embed_len, 0);

    t0 = chrono::high_resolution_clock::now();
    ret = run_imgenc(&rknn_app_ctx, resized.data, img_vec.data());
    if (ret != 0) {
        printf("{\"ok\":false,\"error\":\"run_imgenc failed ret=%d\"}\n", ret);
        release_imgenc(&rknn_app_ctx);
        rkllm_destroy(llmHandle);
        return 1;
    }
    t1 = chrono::high_resolution_clock::now();
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
    double llm_infer_ms = chrono::duration_cast<chrono::microseconds>(t1 - t0).count() / 1000.0;

    auto t_total_end = chrono::high_resolution_clock::now();
    double total_ms = chrono::duration_cast<chrono::microseconds>(t_total_end - t_total).count() / 1000.0;

    // Escape the output text for JSON
    string escaped;
    for (char c : g_output) {
        switch (c) {
            case '"':  escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:   escaped += c;
        }
    }

    printf("{\"ok\":true,\"text\":\"%s\",\"total_ms\":%.0f,\"imgenc_ms\":%.0f,"
           "\"llm_ms\":%.0f,\"llm_load_ms\":%.0f,\"enc_load_ms\":%.0f}\n",
           escaped.c_str(), total_ms, imgenc_ms, llm_infer_ms,
           llm_load_ms, enc_load_ms);

    release_imgenc(&rknn_app_ctx);
    rkllm_destroy(llmHandle);
    return 0;
}
