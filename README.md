# Immich 3DS

**Nintendo 3DS 向けネイティブ Immich クライアント（Homebrew）**

CFW（boot9strap + Luma3DS）を導入した Nintendo 3DS / 3DS LL から、自宅の [Immich](https://immich.app/) サーバーへ Wi-Fi 経由で写真を自動同期・閲覧できるネイティブ C++ クライアントです。

Web アプリケーションではなく、3DS のハードウェア（ARM11 MPCore、PICA200 GPU、タッチスクリーン、標準カメラ DCIM）をフル活用するネイティブ Homebrew として設計されています。

---

## 主な特徴

* **Nintendo 3DS カメラ完全対応**:
  * `sdmc:/DCIM/` 内の撮影済み写真を自動検出（`100NIN03` 等のサブフォルダを再帰走査）。
  * 標準 **JPEG 写真 (`.JPG` / `.JPEG`)** のみを確実にバックアップ（MPO等の非JPEGファイルは安全にスキップ）。
* **二重の重複防止システム (Smart Deduplication)**:
  * ローカルの同期データベース (`sync.json`) による高速判定。
  * Immich の `POST /api/assets/bulk-upload-check` API と SHA-1 ハッシュによるサーバー側二重チェック。
  * 既にサーバーに存在する写真を再アップロードせず、無駄な通信とバッテリー消費を防止。
* **Old 3DS 徹底最適化**:
  * Old 3DS / 3DS LL の実効 RAM（約 64MB）と 268MHz CPU を前提に設計。
  * 超軽量な C 言語製 `cJSON` およびストリーミングアップロード (`curl_mime`) を採用し、メモリ枯渇 (OOM) を防止。
* **強固なネットワークスタック**:
  * `3ds-curl` + `mbedTLS` による正規 HTTPS 通信（Mozilla ルート CA 証明書 `cacert.pem` を romfs に内包）。
  * 自宅 LAN 環境での自己署名証明書（オレオレ証明書）や HTTP 接続にも対応（設定で SSL 検証の切替が可能）。
* **直感的な上下画面専用 UI**:
  * 上画面 (400x240): 接続状態、Wi-Fi 電波強度、写真統計、リアルタイム同期進捗バー。
  * 下画面 (320x240): タッチ操作対応の大型ボタン（Sync Now、Photo List、Settings、Gallery）。
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
|  Photos on 3DS: 128                              |
|  Not Synced:    23                               |
|  Last Sync:     2026/09/06 21:30                 |
+--------------------------------------------------+
|  Uploading 5 / 23 (21%)                          |
|  File: HNI_0023.JPG                              |
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
|         [ Photos on SD Card ]          |
+-------------------+--------------------+
|    [ Settings ]   |    [  Gallery  ]   |
+-------------------+--------------------+
|          [ Exit Immich 3DS ]           |
+----------------------------------------+
|  (A) Sync  (X) Photos  (Y) Config      |
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

1. 3DS で **FBI** を起動します。
2. **「Remote Install」** → **「Scan QR Code」** を選択します。
3. GitHub Releases に掲載されている `Immich3DS.cia` の QR コードを 3DS のカメラで読み取ります。
4. インストール完了後、HOME メニューに「Immich 3DS」のアイコンが追加されます。

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

初回起動時、または PC 上で事前に設定ファイルを作成する場合、SD カードの `sdmc:/3ds/Immich3DS/config.json` に以下を記述します：

```json
{
  "server_url": "https://photos.example.com",
  "api_key": "your_immich_api_key_here",
  "ssl_verify": true,
  "auto_sync": false,
  "timeout_sec": 15
}
```

* `server_url`: 自宅サーバーの URL（末尾スラッシュ不要）
  * LAN 内の場合: `http://192.168.1.100:2283`
  * 外部公開 / ドメインの場合: `https://photos.example.com`
* `api_key`: Immich で発行した API キー
* `ssl_verify`: HTTPS 接続時の証明書検証（自己署名証明書をお使いの場合は `false` に設定）
* `auto_sync`: アプリ起動時に未同期写真を自動でバックアップするかどうか
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

### セキュリティに関する注意事項
* Nintendo 3DS のハードウェアには Secure Enclave 等の暗号化ストレージが存在しないため、`config.json` に保存された API キーは平文で SD カード内に保持されます。
* 万一の 3DS 紛失時に備え、Immich 側で「写真アップロードに必要な最低限の権限」のみを付与した API キーを使用することを強く推奨します。

---

## ライセンス

* **Immich 3DS**: MIT License
* **cJSON**: Copyright (c) 2009-2017 Dave Gamble and cJSON contributors (MIT License)
* **sha1**: Standard FIPS 180-1 implementation (Public Domain)
