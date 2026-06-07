"""Map Qwen3-VL text output to risk_hint, labels, and action_hint."""

import re


# Keywords that indicate different risk levels
_FIRE_KEYWORDS = ["火焰", "fire", "着火", "燃烧", "起火", "明火"]
_SMOKE_KEYWORDS = ["烟雾", "smoke", "浓烟", "烟", "冒烟"]
_PERSON_KEYWORDS = ["人员", "person", "人", "有人", "人物", "人员活动"]
_HAZARD_KEYWORDS = ["危险", "hazard", "异常", "安全风险", "报警", "danger", "alarm"]
_NEGATION_PREFIXES = [
    "未", "没有", "无", "不存在", "未发现", "未观察到", "没有发现", "没有观察到",
    "不能", "不确定", "无法", "难以", "不能确定", "无法确定", "无法判断",
    "是否", "不知道",
]


def _is_negated(text: str, keyword: str) -> bool:
    """Check if keyword appears in a negated context (e.g. '未观察到人员')."""
    idx = text.find(keyword)
    if idx < 0:
        return False
    # Check up to 12 characters before the keyword for negation
    # This handles cases like "未观察到人员、烟雾、火焰"
    prefix = text[max(0, idx - 12) : idx]
    if any(neg in prefix for neg in _NEGATION_PREFIXES):
        return True
    # Also check if there's a negation before a comma-separated list containing this keyword
    # Pattern: "未观察到X、Y、keyword" — negation is before the list start
    # Find the nearest sentence/clause boundary before the keyword
    clause_start = max(
        text.rfind("。", 0, idx),
        text.rfind("，", 0, idx),
        text.rfind("；", 0, idx),
        text.rfind("！", 0, idx),
        0,
    )
    clause = text[clause_start:idx]
    return any(neg in clause for neg in _NEGATION_PREFIXES)


def map_risk(vlm_text: str) -> dict:
    """Map VLM output text to risk assessment.

    Returns dict with: risk_hint (0-6), labels (list), explanation (str).
    """
    text_lower = vlm_text.lower()
    labels = []
    risk_hint = 0

    # Check for fire (only if not negated)
    for kw in _FIRE_KEYWORDS:
        if kw.lower() in text_lower and not _is_negated(text_lower, kw.lower()):
            labels.append("fire")
            risk_hint = max(risk_hint, 6)
            break

    # Check for smoke (only if not negated)
    for kw in _SMOKE_KEYWORDS:
        if kw.lower() in text_lower and not _is_negated(text_lower, kw.lower()):
            if "smoke" not in labels:
                labels.append("smoke")
            risk_hint = max(risk_hint, 4)
            break

    # Check for person (only if not negated)
    for kw in _PERSON_KEYWORDS:
        if kw.lower() in text_lower and not _is_negated(text_lower, kw.lower()):
            if "person" not in labels:
                labels.append("person")
            risk_hint = max(risk_hint, 2)
            break

    # Check for explicit hazard language (only if not negated)
    for kw in _HAZARD_KEYWORDS:
        if kw.lower() in text_lower and not _is_negated(text_lower, kw.lower()):
            risk_hint = max(risk_hint, 4)
            break

    # If no labels detected, leave labels empty
    return {
        "risk_hint": risk_hint,
        "labels": labels,
        "explanation": vlm_text,
    }


def map_action(risk_hint: int) -> str:
    """Map risk_hint to action_hint string."""
    if risk_hint >= 6:
        return "keep_alarm"
    if risk_hint >= 3:
        return "verify"
    return "none"
