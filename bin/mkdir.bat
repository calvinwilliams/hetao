mkdir "C:\Program Files\hetao"

mkdir "C:\Program Files\hetao\bin"
copy "..\bin\install.bat" "C:\Program Files\hetao\bin\install.bat" /Y
copy "..\bin\uninstall.bat" "C:\Program Files\hetao\bin\uninstall.bat" /Y
copy "..\expack\openssl-0.9.8a.win32\bin\libeay32.dll" "C:\Program Files\hetao\bin\libeay32.dll" /Y
copy "..\expack\openssl-0.9.8a.win32\bin\ssleay32.dll" "C:\Program Files\hetao\bin\ssleay32.dll" /Y
copy "..\expack\pcre-7.0-win32\bin\pcre3.dll" "C:\Program Files\hetao\bin\pcre3.dll" /Y
copy "..\expack\pcre-7.0-win32\bin\pcreposix3.dll" "C:\Program Files\hetao\bin\pcreposix3.dll" /Y
copy "..\expack\zlib-1.2.3.win32\bin\zlib1.dll" "C:\Program Files\hetao\bin\zlib1.dll" /Y

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
