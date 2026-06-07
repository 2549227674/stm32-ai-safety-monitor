"""Qwen3-VL backend: calls the single-shot C++ binary and parses JSON output."""

import json
import os
import subprocess
import tempfile
import time

DEFAULT_ENCODER = "/home/orangepi/models/qwen3vl_models/qwen3vl_2b_vision.rknn"
DEFAULT_LLM = "/home/orangepi/models/qwen3vl_models/qwen3vl_2b_w8a8_base.rkllm"
DEFAULT_BINARY = "/opt/edge-ai-safety-monitor/opi5-ai/qwen3vl_single_shot"
DEFAULT_CORE_NUM = "3"
DEFAULT_QUESTION = (
    "请用一句话描述画面内容，并判断是否存在人员、烟雾、火焰或明显安全异常。"
)


def run_inference(
    image_bytes: bytes,
    question: str = DEFAULT_QUESTION,
    encoder_path: str = DEFAULT_ENCODER,
    llm_path: str = DEFAULT_LLM,
    binary_path: str = DEFAULT_BINARY,
    core_num: str = DEFAULT_CORE_NUM,
) -> dict:
    """Run Qwen3-VL inference on image bytes. Returns dict with ok/text/timing."""
    with tempfile.NamedTemporaryFile(suffix=".jpg", delete=False) as f:
        f.write(image_bytes)
        tmp_path = f.name

    try:
        result = subprocess.run(
            [binary_path, tmp_path, encoder_path, llm_path, question, core_num],
            capture_output=True,
            text=True,
            timeout=120,
        )

        # The binary outputs diagnostic info on stderr and JSON on the last line of stdout
        output = result.stdout.strip()
        if not output:
            return {
                "ok": False,
                "error": f"no stdout, stderr={result.stderr[-500:] if result.stderr else ''}",
            }

        # Find the JSON line (last line starting with {)
        json_line = None
        for line in reversed(output.split("\n")):
            line = line.strip()
            if line.startswith("{"):
                json_line = line
                break

        if not json_line:
            return {"ok": False, "error": f"no JSON in stdout: {output[-300:]}"}

        parsed = json.loads(json_line)
        return parsed

    except subprocess.TimeoutExpired:
        return {"ok": False, "error": "inference timeout (120s)"}
    except json.JSONDecodeError as e:
        return {"ok": False, "error": f"JSON parse error: {e}"}
    except Exception as e:
        return {"ok": False, "error": str(e)}
    finally:
        os.unlink(tmp_path)


if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("Usage: python3 qwen3vl_backend.py <image_path> [question]")
        sys.exit(1)

    img_path = sys.argv[1]
    q = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_QUESTION

    with open(img_path, "rb") as f:
        data = f.read()

    t0 = time.time()
    result = run_inference(data, question=q)
    elapsed = time.time() - t0

    print(json.dumps(result, ensure_ascii=False, indent=2))
    print(f"\nwall_time_s: {elapsed:.1f}")
