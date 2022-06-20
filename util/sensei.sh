# sensei.sh - teach ninja to build
#
#   sensei.sh is a ninja syntax generator library written in POSIX-compliant sh.
#   The aim is to make shell-script ninja generators look a little nicer. Note
#   that arbitrary user input is only handled correctly as format placeholder
#   arguments.
#
# shellcheck shell=sh disable=SC3043

NL='
'

err() { printf >&2 'error: %s\n' "$1"; }
die() { printf >&2 'error: %s\n' "$1"; exit 1; }
warn() { printf >&2 'warning: %s\n' "$1"; }
echo() { printf '%s\n' "$1"; }

_starts_with() {
  case "$1" in
    "$2"*) true ;;
    *) false ;;
  esac
}

_contains() {
  case "$1" in
    *"$2"*) true ;;
    *) false ;;
  esac
}

# _format_append_raw <str>
#
#   Append <str> to $FORMAT_RESULT.
#
_format_append_raw() {
  FORMAT_RESULT="$FORMAT_RESULT$1"
}

# _format_append <str>
#
#   Quote and append <str> to $FORMAT_RESULT. Uses $FORMAT_CONTEXT.
#
_format_append() {
  case "$FORMAT_CONTEXT" in
    var) _format_append_var "$1" ;;
    list) _format_append_list "$1" ;;
    *) warn "invalid \$FORMAT_CONTEXT: $FORMAT_CONTEXT" ;;
  esac
}

_format_append_var() {
  local str
  str=$1
  case "$str" in
    # choke on newlines
    *"$NL"*) die "ninja variables may not contain newlines" ;;
  esac
  case "$str" in
    # escape all '$'
    *'$'*) str="$(echo "$str" | sed 's/\$/$$/g')" ;;
  esac
  case "$str" in
    # escape leading space
    ' '*) str="\$$str" ;;
  esac
  _format_append_raw "$str"
}

_format_append_list() {
  local str
  str=$1
  case "$str" in
    # choke on newlines and '|'
    *"$NL"*|*'|'*) die "ninja paths may not contain newlines or '|'" ;;
  esac
  case "$str" in
    # escape all spaces, '$', and ':'
    *[\ :\$]*) str="$(echo "$str" | sed 's/[ :\$]/$&/g')" ;;
  esac
  _format_append_raw "$str"
}

# format_parse -var|-list <format> [<arg>...]
#
#   Parse a format string. This function will terminate the shell with a nonzero
#   status code if it encounters an error. On return, $FORMAT_SHIFT is set to
#   the number of arguments consumed and $FORMAT_RESULT is set to the resulting
#   expansion.
#
#   Format strings may contain escape sequences starting with '%'. The following
#   escape sequences are currently recognized:
#
#     %%      insert a literal '%'
#     %{var}  substitute a ninja variable
#     %var    same as %{var}
#     %{}     substitute a quoted argument
#     %       shorthand for %{}, but may be interpreted as one of the previous
#             if possible
#
#   NOTE: It is generally assumed that you (the developer) are in control of the
#   format string. Arbitrary input should be protected behind the % or %{}
#   specifiers.
#
format_parse() {
  FORMAT_CONTEXT=${1#-}
  FORMAT_SHIFT=0
  FORMAT_RESULT=
  FORMAT=$2
  shift 2

  while _contains "$FORMAT" %; do
    # append the part before '%'
    _format_append "${FORMAT%%%*}"
    # isolate the part after '%'
    FORMAT=${FORMAT#*%}

    case "$FORMAT" in
      # %%
      %*)
        _format_append_raw %
        FORMAT=${FORMAT#%}
        ;;
      # %{}
      \{\}*)
        [ "$#" -eq 0 ] && die "no argument for placeholder"
        _format_append "$1"
        shift
        FORMAT_SHIFT=$((FORMAT_SHIFT + 1))
        FORMAT=${FORMAT#\{\}}
        ;;
      # %{var} and %var
      [\{0-9A-Za-z_]*)
        _format_append_raw '$'
        # let _format_append handle the rest of the variable
        ;;
      # %
      *)
        [ "$#" -eq 0 ] && die "no argument for placeholder"
        _format_append "$1"
        shift
        FORMAT_SHIFT=$((FORMAT_SHIFT + 1))
        ;;
    esac
  done
  # tack on the rest of $FORMAT
  _format_append "$FORMAT"
}

# var <name> <format>
#
#   Set a global ninja variable.
#
var() {
  local name format
  name=$1
  format=$2
  shift 2
  FORMAT_CONTEXT=var
  format_parse -var "$format" "$@"
  shift "$FORMAT_SHIFT"
  [ "$#" -gt 0 ] && \
    die "too many arguments"
  echo "$name = $FORMAT_RESULT"
}

# def <rule> <command> [-<var> <format>...]
#
#   Produce a 'rule' block.
#
def() {
  local rule format
  rule=$1
  format=$2
  shift 2

  echo "rule $rule"
  format_parse -var "$format" "$@"
  shift "$FORMAT_SHIFT"
  echo "  command = $FORMAT_RESULT"

  while [ "$#" -gt 0 ]; do
    if ! _starts_with "$1" -; then
      echo >&2 "in def $rule:"
      die "variables must be prefixed with -"
    fi
    var=${1#-}
    shift
    if [ "$#" -eq 0 ]; then
      echo >&2 "in def $rule:"
      die "variable has no value: $var"
    fi
    format=$1
    shift
    format_parse -var "$format" "$@"
    shift "$FORMAT_SHIFT"
    echo "  $var = $FORMAT_RESULT"
  done
}

# use <rule> <option>...
#
#   Produce a 'build' line.
#
#   -o        output mode (default)
#   -io       implicit output mode
#   -i        input mode
#   -ii       implicit input mode
#   -wi       weak (order-only) input mode
#   -v        validation mode
#   -set <var> <format>
#             set ninja variable
#   <format>  add file
#
use() {
  local rule mode arg name format
  rule=$1
  mode=o
  shift

  local out iout in iin win val var
  out=
  iout=
  in=
  iin=
  win=
  val=
  var=

  while [ "$#" -gt 0 ]; do
    arg=$1
    shift
    case "$arg" in
      -o|-io|-i|-ii|-wi|-v)
        mode=${arg#-}
        ;;
      -set)
        if [ "$#" -lt 2 ]; then
          echo >&2 "in use $rule:"
          die "usage: -set <name> <format>"
        fi
        name=$1
        format=$2
        shift 2
        format_parse -var "$format" "$@"
        shift "$FORMAT_SHIFT"
        var="$var$NL  $name = $FORMAT_RESULT"
        ;;
      -*)
        echo >&2 "in use $rule:"
        die "invalid option: $arg"
        ;;
      *)
        format_parse -list "$arg" "$@"
        shift "$FORMAT_SHIFT"
        case "$mode" in
          o) out="$out $FORMAT_RESULT" ;;
          io) iout="$iout $FORMAT_RESULT" ;;
          i) in="$in $FORMAT_RESULT" ;;
          ii) iin="$iin $FORMAT_RESULT" ;;
          wi) win="$win $FORMAT_RESULT" ;;
          v) val="$val $FORMAT_RESULT" ;;
        esac
        ;;
    esac
  done

  if [ -z "$out" ]; then
    echo >&2 "in use $rule:"
    die "no outputs"
  fi

  echo "build${out}${iout:+ |$iout}: ${rule}${in}${iin:+ |$iin}${win:+ ||$win}${val:+ |@$val}${var}"
}
