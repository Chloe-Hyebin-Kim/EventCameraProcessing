#!/usr/bin/env python3
"""
ShotRecorder가 남긴 CSV를 읽어 Phase 0/1 분석 그래프를 그린다.

입력 (ShotRecorder가 outputDir 아래에 만든 것):
    <outputDir>/measurements.csv          윈도우별 측정값
    <outputDir>/shot_0001/events.csv      트리거 전후 raw 이벤트 (t_us,x,y,p)
    <outputDir>/shot_0001/shot_meta.txt   덤프 구간 메타데이터

사용법:
    python scripts/analyze_shot.py output
    python scripts/analyze_shot.py output --shot 1
    python scripts/analyze_shot.py output --shot 1 --window-us 200
    python scripts/analyze_shot.py output --no-plot        # 숫자만 출력

의존성:
    numpy (필수), matplotlib (그래프를 그릴 때만)

    pip install numpy matplotlib

주의:
    이 스크립트는 관측된 값을 그대로 보여줄 뿐, 스핀을 추정하지 않는다.
    Phase 0에서 답해야 할 질문은 "딤플이 이벤트를 만드는가"이고,
    그 1차 관측량은 공 ROI 안의 이벤트 수와 그 시간적 구조다.
"""

import argparse
import csv
import math
import os
import sys

try:
    import numpy as np
except ImportError:
    sys.exit("numpy가 필요합니다:  pip install numpy")


# ----------------------------------------------------------------- 파일 읽기

def read_measurements(path):
    """measurements.csv를 dict of numpy arrays로 읽는다."""
    with open(path, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))

    if not rows:
        return None

    cols = {}
    for key in rows[0].keys():
        raw = [r[key] for r in rows]
        try:
            cols[key] = np.array([float(v) if v not in ("", None) else np.nan for v in raw])
        except ValueError:
            cols[key] = np.array(raw, dtype=object)   # reject_reason 같은 문자열 열
    return cols


def read_events(path):
    """events.csv를 (t_us, x, y, p) numpy 배열로 읽는다."""
    data = np.loadtxt(path, delimiter=",", skiprows=1, dtype=np.int64)
    if data.ndim == 1:
        data = data.reshape(1, -1)
    return data[:, 0], data[:, 1], data[:, 2], data[:, 3]


def read_meta(path):
    meta = {}
    if not os.path.exists(path):
        return meta
    with open(path, encoding="utf-8") as f:
        for line in f:
            if "=" in line:
                k, v = line.strip().split("=", 1)
                meta[k] = v
    return meta


# ------------------------------------------------------------------- 요약

def summarize_measurements(m):
    n = len(m["window_start_us"])
    det = m["detected"] > 0.5
    trusted = (m["radius_trusted"] > 0.5) & det

    print(f"\n== measurements.csv ==")
    print(f"  윈도우 수            : {n}")
    print(f"  검출 성공            : {det.sum()} ({100.0 * det.sum() / max(n, 1):.1f} %)")
    print(f"  반지름 신뢰(coverage): {trusted.sum()} ({100.0 * trusted.sum() / max(n, 1):.1f} %)")

    wus = m["window_us"]
    print(f"  윈도우 길이          : 중앙값 {np.median(wus):.0f} us, 범위 {wus.min():.0f}~{wus.max():.0f} us")
    if wus.max() - wus.min() > 0.2 * max(np.median(wus), 1):
        print("    [경고] 윈도우 길이가 20 % 넘게 흔들립니다. 벽시계 기반으로 자르고 있지 않은지 확인하세요.")
        print("           108 m/s에서 1 ms = 10.8 cm 입니다.")

    ev = m["event_count"]
    print(f"  윈도우당 이벤트      : 중앙값 {np.median(ev):.0f}, 최대 {ev.max():.0f}")
    on, off = m["on_count"].sum(), m["off_count"].sum()
    tot = on + off
    if tot > 0:
        print(f"  ON / OFF 비율        : {on / tot:.3f} / {off / tot:.3f}")

    if det.sum() > 0:
        print(f"  반지름 (검출된 것)   : 중앙값 {np.median(m['radius_px'][det]):.2f} px, "
              f"표준편차 {np.std(m['radius_px'][det]):.2f} px")
        if trusted.sum() > 2:
            r = m["radius_px"][trusted]
            rel = np.std(r) / max(np.median(r), 1e-9)
            print(f"  반지름 (신뢰분만)    : 중앙값 {np.median(r):.3f} px, 표준편차 {np.std(r):.3f} px")
            print(f"    -> 이 산포가 그대로 거리/볼스피드 오차입니다: 약 {100 * rel:.2f} %")
            print(f"       (dZ/Z = -dr/r. 상용 런치모니터 수준을 노리면 1 % 아래여야 합니다)")
        print(f"  적합 coverage        : 중앙값 {np.median(m['fit_coverage'][det]):.2f}")
        print(f"  원형도 circularity   : 중앙값 {np.median(m['circularity'][det]):.2f}")
        print(f"  적합 rms             : 중앙값 {np.median(m['fit_rms_px'][det]):.2f} px")

    if "reject_reason" in m and det.sum() < n:
        reasons = {}
        for i in range(n):
            if not det[i]:
                key = str(m["reject_reason"][i])
                reasons[key] = reasons.get(key, 0) + 1
        print("  미검출 사유          : " + ", ".join(f"{k}={v}" for k, v in sorted(reasons.items())))

    trig = np.where(m["just_triggered"] > 0.5)[0]
    for i in trig:
        print(f"  트리거               : t = {m['window_end_us'][i]:.0f} us")


def summarize_events(t, x, y, p, meta, ball_px_hint):
    span = t.max() - t.min()
    print(f"\n== events.csv ==")
    print(f"  이벤트 수            : {len(t):,}")
    print(f"  시간 구간            : {t.min()} ~ {t.max()} us  (길이 {span} us = {span / 1000:.2f} ms)")
    print(f"  평균 이벤트율        : {len(t) / max(span, 1) * 1e6 / 1e6:.2f} Mev/s")
    print(f"  ON / OFF             : {(p > 0).sum():,} / {(p <= 0).sum():,}")

    if meta:
        trig = int(meta.get("trigger_us", 0))
        before = (t < trig).sum()
        print(f"  트리거 시각          : {trig} us")
        print(f"  트리거 이전 이벤트   : {before:,} ({100.0 * before / max(len(t), 1):.1f} %)")
        if meta.get("pre_trigger_truncated") == "1":
            print("    [경고] 링버퍼가 요청한 프리트리거 구간의 앞부분을 이미 버렸습니다.")
            print("           ShotRecorderConfig의 ring.spanUs 또는 ring.maxEvents를 키우세요.")
        if int(meta.get("ring_dropped_by_capacity", 0)) > 0:
            print(f"    [경고] 메모리 상한으로 {int(meta['ring_dropped_by_capacity']):,}개 이벤트가 버려졌습니다.")
            print("           ring.maxEvents를 키우거나 ROI/bias로 이벤트율을 낮추세요.")

    if ball_px_hint:
        # 로드맵 03절: 딤플당 픽셀 = 공 지름 px / 10.3
        ppd = ball_px_hint / 10.3
        print(f"\n  공 지름 {ball_px_hint:.1f} px 기준 딤플당 {ppd:.2f} px")
        if ppd < 2.0:
            print("    Nyquist 미만입니다. 딤플 패턴이 원리적으로 복원되지 않습니다.")
        elif ppd < 3.0:
            print("    Nyquist는 넘었지만 여유가 없습니다. 거리를 줄이거나 초점거리를 늘리세요.")
        else:
            print("    현실 최소치(3 px)를 넘었습니다.")
        print("    주의: 딤플 주기 4.13 mm는 388개 균일 육방배열 가정에서 나온 계산값입니다.")
        print("          실제 시험구를 매크로 촬영해 측정한 값으로 바꿔 쓰세요.")


def slice_report(t, x, y, p, window_us, meta):
    """짧은 윈도우로 다시 잘랐을 때 윈도우당 이벤트가 몇 개나 되는지 본다.
    Phase 0에서 '이 윈도우 길이로 신호가 충분한가'를 판단하는 근거."""
    if window_us <= 0:
        return None
    t0 = t.min()
    idx = ((t - t0) // window_us).astype(np.int64)
    nbins = idx.max() + 1
    counts = np.bincount(idx, minlength=nbins)
    print(f"\n== {window_us} us 윈도우로 재분할 ==")
    print(f"  윈도우 수            : {nbins}")
    print(f"  윈도우당 이벤트      : 중앙값 {np.median(counts):.0f}, "
          f"5%={np.percentile(counts, 5):.0f}, 95%={np.percentile(counts, 95):.0f}")
    empty = (counts == 0).sum()
    print(f"  빈 윈도우            : {empty} ({100.0 * empty / max(nbins, 1):.1f} %)")

    # 로드맵 04절: 이 윈도우 안에서 공이 몇 도나 도는가
    print("  이 윈도우 안의 회전각:")
    for name, rpm in (("드라이버", 2500), ("웨지", 10000)):
        deg = rpm / 60.0 * (window_us * 1e-6) * 360.0
        print(f"    {name:8s} {rpm:5d} rpm -> {deg:6.2f} deg")
    return counts


# -------------------------------------------------------------------- 그래프

def plot(m, ev, counts, window_us, outpath):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("\nmatplotlib이 없어 그래프를 건너뜁니다:  pip install matplotlib")
        return

    npanel = 2 + (1 if ev is not None else 0) + (1 if counts is not None else 0)
    fig, axes = plt.subplots(npanel, 1, figsize=(11, 3.1 * npanel))
    if npanel == 1:
        axes = [axes]
    k = 0

    tms = m["window_end_us"] / 1000.0
    det = m["detected"] > 0.5
    trusted = (m["radius_trusted"] > 0.5) & det

    ax = axes[k]; k += 1
    ax.plot(tms, m["on_count"], lw=0.9, label="ON")
    ax.plot(tms, m["off_count"], lw=0.9, label="OFF")
    ax.set_ylabel("events / window")
    ax.set_title("Events per window (Phase 0 primary observable)")
    ax.legend(loc="upper left", fontsize=8)
    ax.grid(alpha=0.25)

    ax = axes[k]; k += 1
    ax.plot(tms[det], m["radius_px"][det], ".", ms=3, alpha=0.45, label="all detections")
    if trusted.sum():
        ax.plot(tms[trusted], m["radius_px"][trusted], ".", ms=4,
                label="coverage >= 0.5 (radius trusted)")
        med = np.median(m["radius_px"][trusted])
        ax.axhline(med, ls="--", lw=0.9, label=f"median {med:.2f} px")
    ax.set_ylabel("radius (px)")
    ax.set_xlabel("t (ms)")
    ax.set_title("Sub-pixel radius - this spread IS the ball-speed error")
    ax.legend(loc="upper left", fontsize=8)
    ax.grid(alpha=0.25)

    for i in np.where(m["just_triggered"] > 0.5)[0]:
        for a in axes[:k]:
            a.axvline(m["window_end_us"][i] / 1000.0, color="k", lw=1.0, alpha=0.6)

    if ev is not None:
        t, x, y, p = ev
        ax = axes[k]; k += 1
        step = max(1, len(t) // 200000)
        on = p > 0
        ax.scatter((t[on][::step] - t.min()) / 1000.0, x[on][::step], s=0.4, alpha=0.35, label="ON")
        ax.scatter((t[~on][::step] - t.min()) / 1000.0, x[~on][::step], s=0.4, alpha=0.35, label="OFF")
        ax.set_ylabel("x (px)")
        ax.set_xlabel("t - t0 (ms)")
        ax.set_title(f"Dumped events, x vs t (showing {len(t) // step:,} of {len(t):,})")
        ax.legend(loc="upper left", fontsize=8, markerscale=12)
        ax.grid(alpha=0.25)

    if counts is not None:
        ax = axes[k]; k += 1
        ax.plot(np.arange(len(counts)) * window_us / 1000.0, counts, lw=0.8)
        ax.set_ylabel(f"events / {window_us} us")
        ax.set_xlabel("t - t0 (ms)")
        ax.set_title(f"Re-sliced at {window_us} us windows")
        ax.grid(alpha=0.25)

    fig.tight_layout()
    fig.savefig(outpath, dpi=130)
    print(f"\n그래프 저장: {outpath}")


# ---------------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser(description="ShotRecorder 출력 분석")
    ap.add_argument("output_dir", help="ShotRecorderConfig.outputDir")
    ap.add_argument("--shot", type=int, default=1, help="분석할 샷 번호 (기본 1)")
    ap.add_argument("--window-us", type=int, default=0,
                    help="덤프 이벤트를 이 길이로 재분할해 통계를 낸다 (예: 200)")
    ap.add_argument("--ball-px", type=float, default=0.0,
                    help="공 지름(px). 주면 딤플당 픽셀을 함께 계산한다")
    ap.add_argument("--no-plot", action="store_true")
    args = ap.parse_args()

    out = args.output_dir
    mpath = os.path.join(out, "measurements.csv")
    if not os.path.exists(mpath):
        sys.exit(f"찾을 수 없습니다: {mpath}")

    m = read_measurements(mpath)
    if m is None:
        sys.exit("measurements.csv에 데이터 행이 없습니다.")
    summarize_measurements(m)

    ev = None
    counts = None
    shot_dir = os.path.join(out, f"shot_{args.shot:04d}")
    epath = os.path.join(shot_dir, "events.csv")

    ball_px = args.ball_px
    if ball_px <= 0:
        det = m["detected"] > 0.5
        tr = (m["radius_trusted"] > 0.5) & det
        if tr.sum() > 2:
            ball_px = 2.0 * float(np.median(m["radius_px"][tr]))

    if os.path.exists(epath):
        t, x, y, p = read_events(epath)
        meta = read_meta(os.path.join(shot_dir, "shot_meta.txt"))
        summarize_events(t, x, y, p, meta, ball_px)
        ev = (t, x, y, p)
        if args.window_us > 0:
            counts = slice_report(t, x, y, p, args.window_us, meta)
    else:
        print(f"\n(샷 덤프 없음: {epath})")

    if not args.no_plot:
        plot(m, ev, counts, args.window_us, os.path.join(out, f"analysis_shot_{args.shot:04d}.png"))


if __name__ == "__main__":
    main()
