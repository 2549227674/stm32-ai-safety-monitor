"""Qwen3-VL backend: supports single-shot and persistent worker modes.

Mode controlled by QWEN3VL_MODE env var:
  - single_shot: calls C++ binary per request (default, fallback)
  - worker: uses persistent C++ worker process (faster after first request)
"""

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
    encoder_path: str = None,
    llm_path: str = None,
    binary_path: str = None,
    core_num: str = None,
) -> dict:
    """Run Qwen3-VL inference. Dispatches to worker or single-shot based on QWEN3VL_MODE."""
    mode = os.environ.get("QWEN3VL_MODE", "single_shot")

    if mode == "worker":
        return _run_worker(image_bytes, question)
    return _run_single_shot(image_bytes, question, encoder_path, llm_path, binary_path, core_num)


def _run_worker(image_bytes, question):
    """Inference via persistent worker process."""
    try:
        import qwen3vl_worker_client
        return qwen3vl_worker_client.run_inference(image_bytes, question)
    except Exception as e:
        return {"ok": False, "error": f"worker error: {e}"}


def _run_single_shot(image_bytes, question, encoder_path, llm_path, binary_path, core_num):
    """Inference via single-shot C++ binary (original method)."""
    encoder_path = encoder_path or os.environ.get("QWEN3VL_ENCODER", DEFAULT_ENCODER)
    llm_path = llm_path or os.environ.get("QWEN3VL_LLM", DEFAULT_LLM)
    binary_path = binary_path or os.environ.get("QWEN3VL_BINARY", DEFAULT_BINARY)
    core_num = core_num or os.environ.get("QWEN3VL_CORE_NUM", DEFAULT_CORE_NUM)

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

        output = result.stdout.strip()
        if not output:
            return {
                "ok": False,
                "error": f"no stdout, stderr={result.stderr[-500:] if result.stderr else ''}",
            }

        json_line = None
        for line in reversed(output.split("\n")):
            line = line.strip()
            if line.startswith("{"):
                json_line = line
                break

        if not json_line:
            return {"ok": False, "error": f"no JSON in stdout: {output[-300:]}"}

        return json.loads(json_line)

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
