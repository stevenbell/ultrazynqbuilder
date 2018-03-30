SRC_URI += "file://user_uz3eg_pciecc_dp.cfg \
            "

SRC_URI_append = " \                  
	file://idt5901-2017.1-v1.0.patch \ 
"                                     

FILESEXTRAPATHS_prepend := "${THISDIR}/${PN}:"
