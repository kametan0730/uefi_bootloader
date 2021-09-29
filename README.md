# UEFI Bootloader

## リポジトリについて

[mikanos](https://github.com/uchan-nos/mikanos)のMikanLoaderPkgを基に作っています。

MikanLoaderPkgはEDK2を利用しており、ビルドするには謎のシェルスクリプトを読み込んで、buildとかいうコマンドを打たなければなりません。```make all```と打てばgccが動き、バイナリが生成されるのが理想ですが、EDK2を利用している限りそれは実現できそうにないです。
本プロジェクトではgnu-efiを利用しており、makeコマンドを利用したビルドが可能となっています。

多少、自作OSの変更に合わせて改造しているため、そのままではmikanosでは動作しません。

## Requirements
- gcc
- make
- gnu-efi
- ovmf
- qemu-system-x86

## Build
```
sudo apt install build-essential gnu-efi
git clone https://github.com/kametan0730/uefi_bootloader loader
cd loader
```
memory_map.hをどこからか用意して配置
```
make
```

## Run
```
sudo apt install ovmf qemu-system-x86
```
起動するカーネルをfs/下に配置
```
make qemu
```

## 改変箇所
- EDK2の依存をなくし、UEFI関連コードはgnu-efiのものを使用するように
- グラフィック関連のコードを削除
