# Immich 3DS

**Nintendo 3DS 向け非公式ネイティブ Immich クライアント（Homebrew）**

[English Documentation](#english) | **日本語**

> [!NOTE]
> **免責事項 / Disclaimers**:
> - **Immich**: 本プロジェクトは個人によって開発された非公式（Third-party）のオープンソースクライアントです。Immich または FUTO との提携、承認、公式な関係は一切ありません。"Immich" は各権利者の商標です。
> - **Nintendo**: 本ソフトウェアは自作ソフト（Homebrew）です。任天堂株式会社（Nintendo Co., Ltd.）との提携、承認、関係はありません。"Nintendo 3DS" は任天堂株式会社の登録商標です。

CFW（boot9strap + Luma3DS）を導入した Nintendo 3DS / 3DS LL から、自宅の [Immich](https://immich.app/) サーバーへ Wi-Fi 経由で写真を自動同期・閲覧できるネイティブ C++ クライアントです。

Web アプリケーションではなく、3DS のハードウェア（ARM11 MPCore、PICA200 GPU、タッチスクリーン、標準カメラ DCIM）をフル活用するネイティブ Homebrew として設計されています。

---

## 主な特徴

* **Nintendo 3DS カメラ完全対応（写真 & 動画）**:
  * `sdmc:/DCIM/` 内の撮影済みメディアを自動検出（`100NIN03` 等のサブフォルダを再帰走査）。
  * 標準 **JPEG 写真 (`.JPG` / `.JPEG`)** および 3DS 標準カメラ **AVI 動画 (`.AVI`)** のバックアップに対応。
  * **3DS 側での動画再エンコードなし**: 3DS 標準カメラの動画（Motion JPEG + IMA ADPCM）をそのままストリーミングアップロード。Immich サーバー側で自動トランスコード・サムネイル生成されます。
  * **MPO 3D 写真・その他形式の完全除外**: 3DS 独自の MPO 形式や未対応形式は完全にスキップし、安全かつ確実に除外します。
* **二重の重複防止システム (Smart Deduplication)**:
  * ローカルの同期データベース (`sync.json`) による高速判定（メディア種別も記録）。
  * Immich の `POST /api/assets/bulk-upload-check` API と SHA-1 ハッシュによるサーバー側二重チェック。
  * 既にサーバーに存在するメディアを再アップロードせず、無駄な通信とバッテリー消費を防止。
* **Old 3DS 徹底最適化 & 動的タイムアウト**:
  * Old 3DS / 3DS LL の実効 RAM（約 64MB）と 268MHz CPU を前提に設計。
  * ファイル全体を RAM に展開しないストリーミングアップロード (`curl_mime_filedata`) とチャンク単位の SHA-1 計算（32KB バッファ）を採用し、動画ファイルでもメモリ枯渇 (OOM) を防止。
  * 動画など大容量ファイルサイズに応じた動的タイムアウトと低速切断保護を装備。
* **強固なネットワークスタック**:
  * `3ds-curl` + `mbedTLS` による正規 HTTPS 通信（Mozilla ルート CA 証明書 `cacert.pem` を romfs に内包）。
  * 自宅 LAN 環境での自己署名証明書（オレオレ証明書）や HTTP 接続にも対応（設定で SSL 検証の切替が可能）。
* **直感的な上下画面専用 UI**:
  * 上画面 (400x240): 接続状態、Wi-Fi 電波強度、写真・動画の内訳統計、リアルタイム同期進捗バー、アップロード中メディアの種別タグ表示 (`[JPG]` / `[AVI]`)。
  * 下画面 (320x240): タッチ操作対応の大型ボタン（Sync Now、Media List、Settings、Gallery）。
  * 物理ボタン（十字キー、A/B/X/Y、START）でも全操作が可能。

---

## UI レイアウト

### 上画面 (400x240)
```text
+--------------------------------------------------+
| Immich 3DS                          Wi-Fi: [|||] |
+--------------------------------------------------+
|  [●] Server: Connected                           |
+--------------------------------------------------+
|  Media on 3DS: 15 (12 Photos, 3 Videos)          |
|  Not Synced:   3 (P: 2, V: 1)                    |
|  Last Sync:    2026/09/06 21:30                  |
+--------------------------------------------------+
|  Uploading Video 2 / 3...                        |
|  [AVI] HNI_0023.AVI (2400 / 12000 KB, 20%)       |
|  [========================>                      ]
+--------------------------------------------------+
```

### 下画面 (320x240 / タッチ対応)
```text
+----------------------------------------+
|                                        |
|             [  Sync Now  ]             |
|                                        |
+----------------------------------------+
|         [  Media on SD Card ]          |
+-------------------+--------------------+
|    [ Settings ]   |    [  Gallery  ]   |
+-------------------+--------------------+
|          [ Exit Immich 3DS ]           |
+----------------------------------------+
|  (A) Sync  (X) Media  (Y) Config       |
+----------------------------------------+
```

---

## 必要環境

### 3DS 端末
* **対象機種**: Nintendo 3DS, 3DS LL, 2DS, New 3DS, New 3DS LL, New 2DS LL（**Old 3DS 基準**）
* **CFW**: boot9strap + Luma3DS 導入済み
* **SD カード**: FAT32 フォーマット（カメラ写真保存用および設定保存用）
* **インターネット環境**: 2.4GHz 帯 Wi-Fi（802.11b/g）

### Immich サーバー
* Immich v1.110.0 以上（**v3 系最新 API 推奨**）
* LAN 内接続 (`http://192.168.x.x:2283`) または リバースプロキシ経由 HTTPS (`https://photos.example.com`)

---

## Immich サーバー側の設定 & API キー作成

### 1. API キーの作成
1. Immich Web UI にログインします。
2. 右上のユーザーアイコン → **「Account Settings」** (アカウント設定) → **「API Keys」** を開きます。
3. **「New API Key」** をクリックします。
4. 説明（例: `Nintendo 3DS`）を入力します。

### 2. 推奨 API 権限 (Permissions)
Immich 3DS では、セキュリティの観点から管理者フルアクセス権限を必要としません。

| 機能 | 必須パーミッション | 説明 |
| :--- | :--- | :--- |
| **MVP（写真同期・アップロード）** | `asset.upload` | 写真のアップロードおよび事前重複チェックに必須 |
| **接続テスト（ユーザー名表示）** | `user.read` | 接続時に「Connected! User: 〇〇」と表示するために使用 |
| **ギャラリー（写真一覧・閲覧）** | `asset.read` | サーバー上の写真一覧およびサムネイル取得に必須 |

※ 初回設定時は、上記 3 つの権限にチェックを入れるか、または `all`（全権限）を選択して作成してください。

---

## 3DS への導入方法 (3つの方式)

### 方式 1: FBI QR コードインストール (最も手軽・推奨)
CIA リリースファイルを 3DS のカメラで QR コードをスキャンして直接インストールします。

<p align="center">
  <img src="https://github.com/tamo2918/Immich-for-3DS/releases/download/v1.0.0/qr.png" width="200" alt="FBI QR Code">
</p>

1. 3DS で **FBI** を起動します。
2. **「Remote Install」** → **「Scan QR Code」** を選択します。
3. 上記の QR コードを 3DS のカメラで読み取ります。
4. `Install and delete CIA`（または `Install CIA`）を選択してインストールします。
5. インストール完了後、HOME メニューに戻るとプレゼントボックス（アイコン）が届いています。

### 方式 2: 3dslink による無線転送 (開発・デバッグ用)
PC と 3DS が同一 Wi-Fi に接続されている場合、SD カードを抜かずに即座にテスト起動できます。

1. 3DS で **Homebrew Launcher** を起動し、`Y` ボタンを押して 3dslink 受信待機状態にします（画面に 3DS の IP アドレスが表示されます）。
2. PC のターミナルで以下を実行します：
   ```bash
   make send IP=192.168.x.x
   ```
3. 3DS 上でアプリが自動転送・起動します。

### 方式 3: SD カードへの手動配置 (.3dsx)
1. GitHub Releases から `Immich3DS.3dsx` と `Immich3DS.smdh` をダウンロードします。
2. SD カードの `/3ds/Immich3DS/` フォルダ内に配置します：
   ```text
   sdmc:/
   └── 3ds/
       └── Immich3DS/
           ├── Immich3DS.3dsx
           └── Immich3DS.smdh
   ```
3. 3DS の Homebrew Launcher から起動します。

---

## 設定ファイル (`config.json`) の準備

初回起動時にデフォルト設定ファイルが自動生成されます。また、同梱のテンプレート [`config.example.json`](config.example.json) をコピーして、SD カードの `sdmc:/3ds/Immich3DS/config.json` に配置することもできます：

```json
{
  "server_url": "https://photos.example.com",
  "api_key": "your_immich_api_key_here",
  "ssl_verify": true,
  "auto_sync": false,
  "timeout_sec": 15
}
```

* `server_url`: 自宅 Immich サーバーの URL（末尾スラッシュ不要）
  * **LAN 内接続（信頼できる自宅Wi-Fiのみ）**: `http://192.168.1.100:2283`
  * **外部・ドメイン経由**: `https://photos.example.com` （※外部接続時は必ず HTTPS をご利用ください）
* `api_key`: Immich Web UI で発行した API キー（写真アップロード権限のみ付与した個別キーを推奨）
* `ssl_verify`: HTTPS 接続時の証明書検証（自宅オレオレ証明書で接続する場合は `false` に設定）
* `auto_sync`: アプリ起動時に未同期写真を自動でバックアップするかどうか (`true`/`false`)
* `timeout_sec`: 通信タイムアウト秒数（標準: 15）

---

## ビルド方法 (開発者向け)

本プロジェクトは macOS（Apple Silicon Mac 対応）および Linux 上で devkitPro を用いてビルド可能です。

### 依存関係のインストール (devkitPro)
```bash
sudo dkp-pacman -S 3ds-dev 3ds-curl 3ds-mbedtls 3ds-zlib 3ds-citro2d 3ds-citro3d
```

### ビルド実行
```bash
# .3dsx および .smdh の生成
make

# .cia (インストール可能パッケージ) の生成
make cia

# ビルド成果物の削除
make clean
```

### Docker を使用した環境依存なしの 1 コマンドビルド
devkitPro をローカルにインストールしていない場合でも、Docker があれば以下のコマンドだけでビルドできます：
```bash
# .3dsx のビルド
make docker

# .cia のビルド
make docker-cia
```

---

## トラブルシューティング & 注意事項

| 現象 / エラー | 原因と対処法 |
| :--- | :--- |
| **No Wi-Fi / 電波アイコン [X]** | 3DS の無線スイッチが ON になっているか、2.4GHz 帯 Wi-Fi に接続されているか確認してください（3DS は 5GHz 帯に非対応です）。 |
| **Ping failed (Timeout / Could not resolve)** | サーバー URL のスペルミス、または 3DS からサーバーへの経路（ファイアウォール・IP）を確認してください。 |
| **Unauthorized: Invalid API Key (401)** | `config.json` の API キーが正しいか確認してください。 |
| **Forbidden: lacks permission (403)** | API キーに必要な権限（`asset.upload` 等）が付与されていません。Immich Web UI で権限を再設定してください。 |
| **SSL certificate problem (Error 60)** | 自宅サーバーでオレオレ証明書をご利用の場合、設定画面で `SSL Verify` を **OFF** に切り替えるか、`config.json` で `"ssl_verify": false` に設定してください。 |
| **SD card error** | SD カードの空き容量、および書き込み禁止スイッチ（SD アダプタ使用時）を確認してください。ログは `sdmc:/3ds/Immich3DS/log.txt` に出力されます。 |

### セキュリティに関する重要事項
* **APIキーの安全な管理**:
  * Nintendo 3DS ハードウェアには暗号化セキュア領域が存在しないため、`config.json` の API キーは SD カード上に平文で保存されます。
  * `config.json` ファイルを GitHub 等の公開リポジトリにアップロードしたり、他者と共有したり絶対にしないでください。
  * 万一の 3DS 紛失や盗難に備え、管理者権限のキーではなく「写真アップロードに必要な権限のみ」を付与した専用 API キーを使用してください。
* **通信の安全性（HTTP 利用時の注意）**:
  * 暗号化されない `http://` 通信では、同一ネットワーク上の通信傍受により API キーや写真データが漏洩するリスクがあります。
  * 平文 HTTP は信頼できるご自身の自宅 LAN / Wi-Fi 内でのみ使用し、公衆無線 LAN やインターネット経由での平文 HTTP 接続は行わないでください。

---

## ライセンス & クレジット

* **Immich 3DS**: [MIT License](LICENSE) (c) 2026 Immich 3DS Contributors
* 外部ライブラリのライセンス詳細および帰属表示は [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) をご覧ください。
  * **cJSON**: MIT License (Copyright (c) 2009-2017 Dave Gamble and cJSON contributors)
  * **sha1**: Public Domain (Steve Reid)
  * **libctru / citro2d / citro3d**: zlib/libpng License
  * **3ds-curl**: curl License
  * **mbedTLS**: Apache License 2.0
  * **Mozilla Root CA Bundle**: MPL 2.0

---

<a name="english"></a>
# English Documentation

**Unofficial Native Immich Client for Nintendo 3DS (Homebrew)**

**[English](#english) | [日本語 (Japanese)](#immich-3ds)**

> [!NOTE]
> **Disclaimers**:
> - **Immich**: This project is an unofficial, third-party open-source client developed independently. It is not affiliated with, endorsed by, or associated with Immich or FUTO. "Immich" is a trademark of its respective owners.
> - **Nintendo**: This software is homebrew. It is not affiliated with, endorsed by, or associated with Nintendo Co., Ltd. "Nintendo 3DS" is a registered trademark of Nintendo Co., Ltd.

Immich 3DS is a native C++ homebrew client for custom firmware (boot9strap + Luma3DS) Nintendo 3DS / 3DS LL systems that seamlessly backs up photos and standard camera videos to your self-hosted [Immich](https://immich.app/) server over Wi-Fi.

Unlike a web app running in a browser, Immich 3DS is built natively to take full advantage of the 3DS hardware (ARM11 MPCore, PICA200 GPU, touchscreen, and direct SD card DCIM access).

---

## Key Features

* **Complete Nintendo 3DS Camera Support (Photos & Videos)**:
  * Automatically scans DCIM directory (`sdmc:/DCIM/` including subdirectories such as `100NIN03`).
  * Supports standard **JPEG photos (`.JPG` / `.JPEG`)** and 3DS camera **AVI videos (`.AVI`)**.
  * **No video re-encoding on 3DS**: Original Motion JPEG + IMA ADPCM AVI video files are streamed directly to the Immich server. Transcoding and web playback thumbnails are generated server-side.
  * **Strict Exclusion of MPO 3D Photos & Unsupported Formats**: 3DS-proprietary MPO 3D photos and other unsupported file extensions are safely filtered out and ignored.
* **Smart Deduplication**:
  * Local sync database (`sync.json`) tracks synced files, file size, timestamps, and media types.
  * Server-side pre-upload checks (`POST /api/assets/bulk-upload-check`) using SHA-1 checksums prevent redundant uploads and save battery.
* **Old 3DS Hardware Optimization & Dynamic Timeouts**:
  * Designed to run reliably within the tight 64MB RAM and 268MHz CPU limits of the original Old 3DS / 3DS LL.
  * Streaming upload (`curl_mime_filedata`) and 32KB chunked SHA-1 hashing avoid loading full files into memory, preventing Out of Memory (OOM) errors.
  * Dynamic transfer timeout calculation and low-speed disconnect protection guard against sleep mode and intermittent Wi-Fi drops.
* **Robust Networking**:
  * Backed by `3ds-curl` and `mbedTLS` for secure HTTPS transfers with Mozilla root CA bundle (`cacert.pem`) embedded in RomFS.
  * Supports LAN-based plain HTTP or self-signed certificates with a toggleable `SSL Verify` option in settings.
* **Dual-Screen Dedicated Interface**:
  * **Top Screen (400x240)**: Connection status, Wi-Fi signal strength, photo/video inventory statistics, real-time sync progress bar, and active file indicators (`[JPG]` / `[AVI]`).
  * **Bottom Screen (320x240 / Touch)**: Large touch-friendly controls (`Sync Now`, `Media on SD Card`, `Settings`, `Gallery`, `Exit`).
  * Full physical button control support (D-Pad, A/B/X/Y, START).

---

## UI Layout

### Top Screen (400x240)
```text
+--------------------------------------------------+
| Immich 3DS                          Wi-Fi: [|||] |
+--------------------------------------------------+
|  [●] Server: Connected                           |
+--------------------------------------------------+
|  Media on 3DS: 15 (12 Photos, 3 Videos)          |
|  Not Synced:   3 (P: 2, V: 1)                    |
|  Last Sync:    2026/09/06 21:30                  |
+--------------------------------------------------+
|  Uploading Video 2 / 3...                        |
|  [AVI] HNI_0023.AVI (2400 / 12000 KB, 20%)       |
|  [========================>                      ]
+--------------------------------------------------+
```

### Bottom Screen (320x240 / Touch-Enabled)
```text
+----------------------------------------+
|                                        |
|             [  Sync Now  ]             |
|                                        |
+----------------------------------------+
|         [  Media on SD Card ]          |
+-------------------+--------------------+
|    [ Settings ]   |    [  Gallery  ]   |
+-------------------+--------------------+
|          [ Exit Immich 3DS ]           |
+----------------------------------------+
|  (A) Sync  (X) Media  (Y) Config       |
+----------------------------------------+
```

---

## System Requirements

### 3DS Console
* **Compatible Models**: Nintendo 3DS, 3DS LL, 2DS, New 3DS, New 3DS LL, New 2DS LL (Target baseline: **Old 3DS**)
* **Custom Firmware**: boot9strap + Luma3DS
* **SD Card**: FAT32 formatted
* **Wi-Fi**: 2.4GHz 802.11b/g network

### Immich Server
* Immich v1.110.0 or higher (**v3.x API recommended**)
* LAN HTTP access (`http://192.168.x.x:2283`) or reverse-proxy HTTPS (`https://photos.example.com`)

---

## Server Setup & API Key Configuration

1. Log into your Immich Web UI.
2. Go to **Account Settings** → **API Keys** → **New API Key**.
3. Name it (e.g. `Nintendo 3DS`).
4. **Recommended Permissions**:
   * Minimum: `asset.upload` (uploading media & deduplication checks)
   * Status checking: `user.read` (displays username upon connection)
   * Gallery view: `asset.read` (fetches remote asset lists & thumbnails)

---

## Installation (3 Methods)

### Method 1: FBI QR Code Install (Recommended)
Install the standalone CIA package directly onto your 3DS HOME Menu using FBI:

<p align="center">
  <img src="https://github.com/tamo2918/Immich-for-3DS/releases/download/v1.0.0/qr.png" width="200" alt="FBI QR Code">
</p>

1. Launch **FBI** on your Nintendo 3DS.
2. Select **Remote Install** → **Scan QR Code**.
3. Scan the QR code above.
4. Select `Install and delete CIA` (or `Install CIA`).
5. Return to the 3DS HOME Menu to unwrap your new application!

### Method 2: Wireless Transfer via 3dslink (Development / Testing)
1. Open the **Homebrew Launcher** on your 3DS and press **Y** to activate NetLoader.
2. From your computer, run:
   ```bash
   make send IP=192.168.x.x
   ```
3. The app will be streamed and launched automatically.

### Method 3: Manual SD Card Installation (.3dsx)
1. Download `Immich3DS.3dsx` and `Immich3DS.smdh` from [GitHub Releases](https://github.com/tamo2918/Immich-for-3DS/releases).
2. Copy them to your SD card under `/3ds/Immich3DS/`:
   ```text
   sdmc:/
   └── 3ds/
       └── Immich3DS/
           ├── Immich3DS.3dsx
           └── Immich3DS.smdh
   ```
3. Launch via the Homebrew Launcher.

---

## Configuration (`config.json`)

A default configuration is automatically generated on first launch. You can also copy the bundled [`config.example.json`](config.example.json) to `sdmc:/3ds/Immich3DS/config.json`:

```json
{
  "server_url": "https://photos.example.com",
  "api_key": "your_immich_api_key_here",
  "ssl_verify": true,
  "auto_sync": false,
  "timeout_sec": 15
}
```

* `server_url`: Your Immich server address without trailing slashes.
  * Local LAN: `http://192.168.1.100:2283`
  * External HTTPS: `https://photos.example.com`
* `api_key`: Your Immich API Key.
* `ssl_verify`: Set to `false` if using a self-signed certificate on your local LAN.
* `auto_sync`: Set to `true` to immediately start syncing on launch.
* `timeout_sec`: Network request timeout in seconds (default: 15).

---

## Building from Source

### Using devkitPro (Native)
```bash
sudo dkp-pacman -S 3ds-dev 3ds-curl 3ds-mbedtls 3ds-zlib 3ds-citro2d 3ds-citro3d
make          # Build .3dsx and .smdh
make cia      # Build .cia package (requires makerom and bannertool)
```

### Using Docker (Reproducible 1-Command Build)
```bash
make docker   # Builds Immich3DS.3dsx inside the official devkitPro container
```

---

## Security & Best Practices

* **API Key Storage**: Because the Nintendo 3DS lacks a hardware-backed secure enclave, credentials in `config.json` are stored as plaintext on the SD card.
  * Never commit or share your `config.json`.
  * Use a dedicated API key limited to `asset.upload`.
* **Plaintext HTTP**: Connecting over unencrypted HTTP sends API keys and images in the clear. Only use plain HTTP on your trusted home Wi-Fi.

---

## License & Third-Party Notices

* **Immich 3DS**: [MIT License](LICENSE) (c) 2026 Immich 3DS Contributors
* See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for full licensing terms of third-party dependencies (cJSON, SHA-1, libctru, citro2d, citro3d, curl, mbedTLS, Mozilla CA).

