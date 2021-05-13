set(COMPONENT_SRCS "main.c")
set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_REQUIRES 
    mcommon 
    mconfig 
    mwifi 
    mlink 
    mqtt_manager 
    ina219 
)
register_component()
