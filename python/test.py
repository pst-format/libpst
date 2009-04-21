import _libpst, sys

for i in range(1,len(sys.argv)):
    print "try file %s" % (sys.argv[i])
    pst = _libpst.pst(sys.argv[i])
    topf = pst.pst_getTopOfFolders()

    while (topf):
        #print "topf d_id is %d\n" % (topf.d_id)
        item = pst.pst_parse_item(topf, None)
        if (item):
            if (item.type == 1):
                em = item.email
                if (em):
                    if (em.messageid.str):
                        print "message id is |%s|" % (em.messageid.str)
                    subj = item.subject;
                    if (subj.str):
                        print "subject is %s" % (subj.str)
                    body = item.body
                    #if (body.str):
                    #    print "message body is %s" % (body.str)
                    att = item.attach
                    while (att):
                        attid   = att.i_id
                        print "attachment id %d" % (attid)
                        att1 = att.filename1
                        att2 = att.filename2
                        print "attachment file name %s %s" % (att1.str, att2.str)
                        attdata = pst.pst_attach_to_mem(att)
                        if (attdata):
                            print "data size %d" % (len(attdata))
                        att = att.next
        topf = pst.pst_getNextDptr(topf)
    print "done"

