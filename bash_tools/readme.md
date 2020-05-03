SGUG Bash Helper Tools

At the end of the sugshell script there is this hook:

# If ~/.sgug_bashrc exists, use that as our init file.
[ -e $HOME/.sgug_bashrc ] && exec bash --rcfile $HOME/.sgug_bashrc -i

This directory has a dot-sgug_bashrc file for you to use and hopefully make using sgug-rse building easier.


