savedcmd_shellquest_stats.mod := printf '%s\n'   shellquest_stats.o | awk '!x[$$0]++ { print("./"$$0) }' > shellquest_stats.mod
