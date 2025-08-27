import sys
import subprocess

def main():
    if len(sys.argv) < 5:
        print("使い方: python silhouette_ffmpeg_mac.py 入力動画 出力動画 背景モード しきい値")
        print("背景モード: 1=白背景+黒影, 2=黒背景+白影")
        print("しきい値: 0-255 の整数")
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2]
    bg_mode = sys.argv[3]

    try:
        threshold_value = int(sys.argv[4])
        if not (0 <= threshold_value <= 255):
            raise ValueError
    except ValueError:
        print("しきい値は 0-255 の整数で指定してください")
        sys.exit(1)

    if bg_mode not in ("1", "2"):
        print("背景モードは 1 または 2 で指定してください")
        sys.exit(1)

    # 0-255 -> 0.0-1.0 に変換
    threshold_norm = threshold_value / 255.0

    # 黒白反転の有無で背景モードを切替
    if bg_mode == "1":  # 白背景 + 黒影
        vf_filter = f"format=gray,lut='val*0+255*lt(val\\,{threshold_value})'"
    else:                # 黒背景 + 白影
            vf_filter = f"format=gray,lut='val*0+255*gte(val\\,{threshold_value})'" 

    cmd = [
        "ffmpeg",
        "-y",
        "-i", input_path,
        "-vf", vf_filter,
        "-c:a", "copy",  # 音声をコピー
        output_path
    ]

    print("コマンド実行:", " ".join(cmd))
    subprocess.run(cmd)
    print("完成しました:", output_path)


if __name__ == "__main__":
    main()

