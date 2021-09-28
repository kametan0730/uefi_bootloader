# UEFI Bootloader

## リポジトリについて

[mikanos](https://github.com/uchan-nos/mikanos)のMikanLoaderPkgを基に作っています。

MikanLoaderPkgはEDK2を利用しており、ビルドするには謎のシェルスクリプトを読み込んで、buildとかいうコマンドを打たなければなりません。```make all```と打てばgccが動き、バイナリが生成されるのが理想ですが、EDK2を利用している限りそれは実現できそうにないです。
本プロジェクトではgnu-efiを利用しており、makeコマンドを利用したビルドが可能となっています。

多少、自作OSの変更に合わせて改造しているため、そのままではmikanosでは動作しません。

## 改変箇所

- EDK2の依存をなくし、UEFI関連コードはgnu-efiのものを使用するように
- グラフィック関連のコードを削除