add_executable(proxy-client pclient.cpp proxyclient.cpp tcppipe.cpp util.cpp)
target_link_libraries(proxy-client ${DINT} ${CRYPTOPP} pthread logger)

add_executable(proxy-server pserver.cpp proxyserver.cpp tcppipe.cpp util.cpp)
target_link_libraries(proxy-server ${DINT} ${CRYPTOPP} pthread logger)


