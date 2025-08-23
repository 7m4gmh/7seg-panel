// www/script.js

// グローバルな設定値（C++側と合わせる）
const MAX_FILE_SIZE = 100 * 1024 * 1024; // 100MB

async function updateStatus() {
    try {
        const response = await fetch('/status');
        const data = await response.json();
        
        document.getElementById('now-playing-text').textContent = data.now_playing;
        document.getElementById('stop-form').style.display = data.now_playing === '(none)' ? 'none' : 'inline-block';

        const queueList = document.getElementById('queue-list');
        queueList.innerHTML = '';
        if (data.queue.length === 0) {
            queueList.innerHTML = '<li>(empty)</li>';
        } else {
            data.queue.forEach(item => {
                const li = document.createElement('li');
                li.innerHTML = `${item.filename} 
                    <form class="action-form delete-form" action="/delete" method="post" style="display:inline;">
                        <input type="hidden" name="index" value="${item.index}">
                        <input type="submit" class="delete-btn" value="Delete">
                    </form>`;
                queueList.appendChild(li);
            });
        }
    } catch (error) { console.error('Failed to update status:', error); }
}

async function handleActionFormSubmit(event) {
    event.preventDefault();
    const form = event.target;
    
    // ★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★
    // ★★★              ここから修正             ★★★
    // ★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★
    
    // fetchリクエストのオプションを準備
    const fetchOptions = {
        method: 'POST'
    };

    // 'stop-form' 以外の場合（つまりデータを持つフォームの場合）のみ、bodyを設定する
    if (form.id !== 'stop-form') {
        fetchOptions.body = new FormData(form);
    }
    
    // 上記の修正により、/stop へのPOSTリクエストはbodyを持たなくなります。
    // これにより、中身が空の multipart/form-data ではなく、
    // シンプルなPOSTリクエストとして送信され、サーバーが正しく解釈できます。

    try {
        // 修正したオプションを使ってfetchを実行
        await fetch(form.action, fetchOptions);
        updateStatus();
    } catch (error) { 
        console.error('Form submission failed:', error); 
    }
    // ★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★
    // ★★★              ここまで修正             ★★★
    // ★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★
}

function handleUploadFormSubmit(event) {
    event.preventDefault();
    const form = event.target;
    const fileInput = form.querySelector('input[type="file"]');
    
    if (fileInput.files.length === 0) {
        alert('Please select a file to upload.');
        return;
    }

    const file = fileInput.files[0];
    if (file.size > MAX_FILE_SIZE) {
        alert(`Upload failed: File size (${Math.round(file.size / 1024 / 1024)} MB) exceeds the limit of ${MAX_FILE_SIZE / 1024 / 1024} MB.`);
        return;
    }

    const formData = new FormData(form);
    fetch('/upload', { method: 'POST', body: formData })
        .then(response => {
            if (response.ok) {
                form.reset();
                updateStatus();
            } else {
                response.text().then(text => alert('Upload failed: ' + text));
            }
        })
        .catch(error => alert('Upload failed: ' + error));
}

// === イベントリスナーのセットアップ ===

// ページが読み込まれたら初期化処理を実行
document.addEventListener('DOMContentLoaded', () => {
    // ファイルサイズ制限のテキストを動的に設定
    document.getElementById('max-file-size-text').textContent = `Max file size: ${MAX_FILE_SIZE / 1024 / 1024} MB`;

    // イベントデリゲーション: Stop/Deleteフォームの送信を捕捉
    document.addEventListener('submit', function(event) {
        if (event.target.matches('.action-form')) {
            handleActionFormSubmit(event);
        }
    });

    // アップロードフォームの送信を捕捉
    document.getElementById('upload-form').addEventListener('submit', handleUploadFormSubmit);

    // 定期的なステータス更新を開始
    setInterval(updateStatus, 2000);
    // ページ読み込み時に即時更新
    updateStatus();
});

