SRC_URI += "file://user_uz3eg_iocc_dp.cfg \
            file://user_2017-12-05-18-45-00.cfg \
            file://user_2017-12-07-19-26-00.cfg \
            file://user_2017-12-19-19-02-00.cfg \
            file://user_2017-12-22-23-35-00.cfg \
            file://user_2018-01-15-14-31-00.cfg \
            file://user_2018-01-15-15-09-00.cfg \
            file://user_2018-02-09-16-51-00.cfg \
            file://user_2018-03-05-18-07-00.cfg \
            file://user_2018-03-05-18-17-00.cfg \
            "

SRC_URI_append = " \                  
	file://idt5901-2017.1-v1.0.patch \ 
"                                     

FILESEXTRAPATHS_prepend := "${THISDIR}/${PN}:"
