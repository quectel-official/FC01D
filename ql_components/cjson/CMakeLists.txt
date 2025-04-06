
list(APPEND cjson_includes
    ${CMAKE_CURRENT_LIST_DIR}/../../ql_components/cjson

)


# file(GLOB_RECURSE mqtt_client_sources "${CMAKE_CURRENT_LIST_DIR}/../../ql_components/paho-mqtt/client/src/*.c")

# file(GLOB_RECURSE mqtt_ui_sources "${CMAKE_CURRENT_LIST_DIR}/../../ql_components/paho-mqtt/mqtt_ui/*.c")

# file(GLOB_RECURSE mqtt_packet_sources "${CMAKE_CURRENT_LIST_DIR}/../../ql_components/paho-mqtt/packet/src/*.c")

# list(APPEND mqtt_sources
#    ${mqtt_client_sources}
#    ${CMAKE_CURRENT_LIST_DIR}/../../ql_components/paho-mqtt/client/paho_mqtt_udp.c
#    ${mqtt_packet_sources}
#    ${mqtt_ui_sources}
# )

include_directories(BEFORE  ${cjson_includes})

add_library(cjson STATIC ${CMAKE_CURRENT_LIST_DIR}/../../ql_components/cjson/cJSON.c)

