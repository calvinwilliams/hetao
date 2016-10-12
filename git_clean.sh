git filter-branch --force --index-filter 'git rm --cached --ignore-unmatch setup.exe' --prune-empty --tag-name-filter cat HEAD -- --all
git push origin master --force  
rm -Force -R .git/refs/original/  
git reflog expire --expire=now --all  
git gc --prune=now  
git gc --aggressive --prune=now

