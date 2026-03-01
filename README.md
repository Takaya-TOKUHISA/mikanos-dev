# MikanOS 開発記録

『ゼロからのOS自作入門』を進めながら、OSの仕組みを学習するリポジトリ
## 環境
- **OS**: Ubuntu20.04(WSL2)
- **Editor**: VS Code (Remote WSL), Okteta
- **Emulator**: QEMU
## 進捗
### 1日目(2026-03-01)，第一章
- **やったこと**
    - バイナリファイル'BOOTX64.EFI'による"Hello, world!"の表示命令の作成．
    - EDK IIを用いた'BOOT64.EFI'の配置，QEMUでの起動．
    - C環境での"Hello, world!"の表示命令の実現とQEMUでの表示．
- **学び**
    - 文字の表示には空ファイルをFAT形式でフォーマットし，そこにバイナリファイルを書き込む．
    - EDK II環境をとりいえるためにansibleを用いたが，githubのバージョン管理機能をおろそかにしており，Ubuntu20.04に対応するバージョンではなく，エラーに見舞われた．
- **考察**
    - 今までのCのコンパイルはgccで行っており，これは対象のOSに沿った形にコンパイルされる．今回は作成したOS(Ubuntu20.04)である一方実行ファイルのルールはwindows形式であるため，windows向けのバイナリファイルを作成する必要があるという部分で少し複雑に見えると考えた．
    - gccではmainから始めるが，今回はEfiMainを指定してスタートする位置を自分で決めている．
    
---

## Credits
This project is based on the book:
- **"ゼロからのOS自作入門" (MikanOS)** by [Kouta Uchida (uchan-nos)](https://github.com/uchan-nos/mikanos)
- Some build tools and scripts are derived from [osbook/devenv](https://github.com/uchan-nos/mikanos-build).
