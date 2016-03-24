# DESCRIPTION:
#   Print each item of a given list on its own line.
# USAGE:
#   set(
#     lines
#       "line 1"
#       "line 2"
#       "line 3"
#   )
#   println(${lines})

function(println)
  foreach(line ${ARGV})
    message("${line}")
  endforeach()
endfunction()

