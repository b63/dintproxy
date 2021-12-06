add_executable(proxy-client pclient.cpp proxyclient.cpp tcppipe.cpp util.cpp logger.cpp)
target_link_libraries(proxy-client ${DINT} ${CRYPTOPP} pthread)

add_executable(proxy-server pserver.cpp proxyserver.cpp tcppipe.cpp util.cpp logger.cpp)
target_link_libraries(proxy-server ${DINT} ${CRYPTOPP} pthread)


