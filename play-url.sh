#!/bin/bash

# --- 1. 引数のチェック ---
if [ "$#" -ne 2 ]; then
    echo "使い方: $0 \"YouTubeのURL\" \"設定名 (例: 12x8)\""
    exit 1
fi

# --- 2. 変数の設定 ---
URL_INPUT="$1"
CONFIG_NAME="$2"
PLAYER_BIN="./7seg-file-player"
YTDLP_BIN="../led/venv/bin/yt-dlp"

# --- 3. プロセスツリーを強制終了させるための関数 ---
kill_processtree() {
    local top_pid=$1
    if [ -z "$top_pid" ]; then return; fi
    local children=$(pgrep -P "$top_pid")
    for pid in $children; do
        kill_processtree "$pid"
    done
    kill "$top_pid" 2>/dev/null
}

# --- 4. Ctrl+Cやスクリプト終了時のクリーンアップ処理 ---
cleanup() {
    echo
    echo "クリーンアップ処理を実行中..."
    if [ -n "$YTDLP_PID" ]; then
        kill_processtree "$YTDLP_PID"
    fi
    # 端末設定を元に戻す (対話モードで実行された場合のみ)
    if [ -t 0 ]; then
        stty echo
    fi
    exit 0
}
trap cleanup INT TERM

# --- 5. メインの処理 ---
echo "プレイリストから全てのURLを取得しています..."
readarray -t URL_LIST < <( "$YTDLP_BIN" --flat-playlist -j "$URL_INPUT" | jq -r '.url' )

if [ ${#URL_LIST[@]} -eq 0 ]; then
    echo "エラー: 有効な動画URLが取得できませんでした。"
    exit 1
fi

# ★★★ ここからが核心 ★★★
# 対話モードか (キーボードが使えるか) どうかを判定
if [ -t 0 ]; then
    IS_INTERACTIVE=true
    echo "対話モードで再生を開始します ('n'でスキップ, 'q'かCtrl+Cで終了)"
else
    IS_INTERACTIVE=false
    echo "非対話モード (nohupなど) で再生を開始します。スキップ機能は無効です。"
fi

# 配列をループする
for video_url in "${URL_LIST[@]}"; do
    echo "-----------------------------------------------------"
    echo "再生中: $video_url"
    echo "-----------------------------------------------------"

    # パイプライン全体をバックグラウンドで実行
    "$YTDLP_BIN" \
        -f "bestvideo+bestaudio/best" \
        --fragment-retries 10 \
        -o - \
        "$video_url" \
    | ffmpeg -hide_banner -loglevel error -i - -c copy -f matroska pipe:1 \
    | "$PLAYER_BIN" - "$CONFIG_NAME" &
    
    YTDLP_PID=$!

    # --- モードに応じて待機方法を切り替える ---
    if $IS_INTERACTIVE; then
        # 【対話モード】キー入力を監視する
        stty -echo -cbreak
        while kill -0 "$YTDLP_PID" 2>/dev/null; do
            read -s -n 1 -t 0.5 key
            if [[ $? -eq 0 ]]; then
                if [[ "$key" == "n" ]]; then
                    echo "スキップします..."
                    kill_processtree "$YTDLP_PID"
                    break 
                elif [[ "$key" == "q" ]]; then
                    echo "終了します..."
                    cleanup
                fi
            fi
        done
        stty echo
    else
        # 【非対話モード】ただプロセスの終了を待つだけ
        wait "$YTDLP_PID" 2>/dev/null
    fi
    
    YTDLP_PID="" # PIDをクリア

    if [[ "$video_url" != "${URL_LIST[-1]}" ]]; then
        echo "動画が終了しました。次の動画へ移ります..."
        sleep 1
    fi
done

echo "プレイリストの再生が完了しました。"