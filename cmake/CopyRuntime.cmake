cmake_minimum_required(VERSION 3.24)


# Copy resolved runtime dependencies beside the requesting executable.
foreach(library IN LISTS LIBRARIES)
  get_filename_component(name "${library}" NAME)
  file(COPY_FILE "${library}" "${DESTINATION}/${name}" ONLY_IF_DIFFERENT)
endforeach()
