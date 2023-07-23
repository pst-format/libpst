import sys
import _libpst

ft = _libpst.FILETIME()
ft.dwLowDateTime = 0
ft.dwHighDateTime = 1


for i in range(1,len(sys.argv)):
    print "try file %s" % (sys.argv[i])
    pst = _libpst.pst(sys.argv[i], "")
    topf = pst.pst_getTopOfFolders()

    print pst.pst_rfc2425_datetime_format(ft)
    print pst.pst_rfc2445_datetime_format(ft)

    while (topf):
        #print "topf d_id is %d\n" % (topf.d_id)
        item = pst.pst_parse_item(topf, None)
        if (item):
            if (item.type == 1):
                em = item.email
                if (em):
                    print "default charset %s" % (pst.pst_default_charset(item))
                    ft = em.arrival_date
                    if (ft):
                        print "message arrived at %s" % (pst.pst_rfc2425_datetime_format(ft))
                    if (em.messageid.str):
                        print "message id is <%s>" % (em.messageid.str)
                    subj = item.subject
                    rtf  = em.rtf_compressed
                    if rtf and (len(rtf) > 0): print "rtf compressed size", len(rtf)
                    if (subj and subj.str):
                        was = subj.is_utf8
                        pst.pst_convert_utf8(item, subj)
                        now = subj.is_utf8
                        if (was != now):
                            print "subject was converted to utf8"
                        print "subject is %s" % (subj.str)
                    body = item.body
                    #if (body.str):
                    #    print "message body is %s" % (body.str)
                    att = item.attach
                    att = None
                    while (att):
                        attid = att.i_id
                        att1  = att.filename1
                        att2  = att.filename2
                        print "attachment id %d file name short '%s' long '%s'" % (attid, att1.str, att2.str)
                        if (1):
                            attdata = pst.pst_attach_to_mem(att)
                            if (attdata):
                                print "data size %d" % (len(attdata))
                        if (0):
                            f = pst.ppst_open_file(att2.str, 'w')
                            if (f):
                                si = pst.pst_attach_to_file_base64(att, f)
                                pst.ppst_close_file(f)
                                print "wrote %d bytes in %s" % (si, att2.str)
                        if (0):
                            f = pst.ppst_open_file(att2.str, 'w')
                            if (f):
                                si = pst.pst_attach_to_file(att, f)
                                pst.ppst_close_file(f)
                                print "wrote %d bytes in %s" % (si, att2.str)
                        att = att.next
            pst.pst_freeItem(item)
        topf = pst.pst_getNextDptr(topf)
    print "done"

