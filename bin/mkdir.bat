mkdir "C:\Program Files\hetao"

mkdir "C:\Program Files\hetao\bin"
copy "..\bin\install.bat" "C:\Program Files\hetao\bin\install.bat" /Y
copy "..\bin\uninstall.bat" "C:\Program Files\hetao\bin\uninstall.bat" /Y

mkdir "C:\Program Files\hetao\conf"
if exist "C:\Program Files\hetao\conf\hetao.conf" goto skip_copy_hetao_conf
copy "..\conf\hetao.conf.WINDOWS" "C:\Program Files\hetao\conf\hetao.conf" /Y
:skip_copy_hetao_conf

mkdir "C:\Program Files\hetao\www"
if exist "C:\Program Files\hetao\www\index.html" goto skip_copy_index_html
copy "..\www\index.html" "C:\Program Files\hetao\www\index.html" /Y
:skip_copy_index_html
if exist "C:\Program Files\hetao\www\error_pages" goto skip_copy_error_pages
mkdir "C:\Program Files\hetao\www\error_pages"
xcopy "..\www\error_pages" "C:\Program Files\hetao\www\error_pages"
:skip_copy_error_pages

mkdir "C:\Program Files\hetao\log"

pause
