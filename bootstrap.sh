# bootstrap.sh
# bootstrapping environment setup script
#
# This should be sourced only for first-time installations. Only environment
# variables *absolutely necessary* for correctly installing corecon should be
# declared here.

# explicitly specify XDG dirs
export XDG_CONFIG_HOME="$HOME/.config"
export XDG_DATA_HOME="$HOME/.data"
export XDG_STATE_HOME="$HOME/.state"
export XDG_CACHE_HOME="$HOME/.cache"

# prepend entries to $PATH
export PATH
for ent in ~/.local/bin
do
  case :"$PATH": in
    *:"$ent":*) ;;
    *) PATH="$ent:$PATH" ;;
  esac
done
unset ent
