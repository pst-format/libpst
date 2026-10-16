/*
 * Regression test for readpst: a message without transport headers that
 * carries two message/rfc822 attachments, built in memory, no PST needed.
 *
 * write_normal_email() used to leave extra_mime_headers pointing into the
 * first embedded message's own email->header. write_embedded_message() frees
 * that item right after, and find_rfc822_headers() then read (and could
 * write) the freed buffer while handling the second embedded message.
 *
 * readpst.c is compiled into this program with main() renamed, and with
 * pst_getID() and pst_parse_item() renamed to the stubs below, so that
 * write_embedded_message() gets its embedded items from memory instead of a
 * PST file. Everything else, including pst_freeItem(), is the real code.
 *
 * The test fails if:
 *   - when the second embedded message is parsed, extra_mime_headers still
 *     points into the first embedded message, which has been freed by then;
 *   - extra_mime_headers is not NULL once the outer message is written;
 *   - the output does not contain both embedded messages and a closed outer
 *     multipart.
 * Under AddressSanitizer or valgrind the read of freed memory is also
 * reported directly, before the checks above run.
 */

#define main           readpst_main
#define pst_getID      test_pst_getID
#define pst_parse_item test_pst_parse_item
#include "readpst.c"
#undef main
#undef pst_getID
#undef pst_parse_item

#include <stdint.h>

/* readpst.c calls write_msg_email() (msg.cpp) for -m output; never used here */
void write_msg_email(char *fname, pst_item* item, pst_file* pst)
{
    (void)fname; (void)item; (void)pst;
    abort();
}

#define N_EMBEDDED 2

/* Each embedded message has its own headers, with a MIME block after the
 * blank line, as an Outlook-saved transport header often has. */
static const char *embedded_headers[N_EMBEDDED] = {
    "Received: from mx.example.org by mx.example.net; Mon, 1 Jan 2024 10:00:00 +0000\r\n"
    "From: First Sender <first@example.org>\r\n"
    "To: Recipient <rcpt@example.net>\r\n"
    "Subject: first embedded message\r\n"
    "Date: Mon, 1 Jan 2024 10:00:00 +0000\r\n"
    "Message-Id: <first@example.org>\r\n"
    "Content-Type: multipart/mixed; boundary=\"inner-1\"\r\n"
    "\r\n"
    "--inner-1\r\n"
    "Content-Type: text/plain; charset=us-ascii\r\n",

    "Received: from mx.example.org by mx.example.net; Tue, 2 Jan 2024 10:00:00 +0000\r\n"
    "From: Second Sender <second@example.org>\r\n"
    "To: Recipient <rcpt@example.net>\r\n"
    "Subject: second embedded message\r\n"
    "Date: Tue, 2 Jan 2024 10:00:00 +0000\r\n"
    "Message-Id: <second@example.org>\r\n"
    "Content-Type: multipart/mixed; boundary=\"inner-2\"\r\n"
    "\r\n"
    "--inner-2\r\n"
    "Content-Type: text/plain; charset=us-ascii\r\n",
};

static char     *extra_mime_headers_under_test;
static uintptr_t header_start[N_EMBEDDED];
static uintptr_t header_end[N_EMBEDDED];
static int       parsed;
static int       failures;

static void fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    failures++;
}

/* true if p points into the headers of embedded message i (compared as
 * integers, so that nothing is dereferenced after the free) */
static int points_into(const char *p, int i)
{
    uintptr_t u = (uintptr_t)p;
    return p && u >= header_start[i] && u <= header_end[i];
}

static void set_str(pst_string *s, const char *value)
{
    s->str     = strdup(value);
    s->is_utf8 = 1;
}

static pst_item *new_email(uint64_t block_id, const char *subject, const char *body)
{
    pst_item *item = calloc(1, sizeof(pst_item));
    if (!item) abort();
    item->email = calloc(1, sizeof(pst_item_email));
    if (!item->email) abort();
    item->type     = PST_TYPE_NOTE;
    item->block_id = block_id;
    set_str(&item->subject, subject);
    set_str(&item->body, body);
    return item;
}

pst_index_ll *test_pst_getID(pst_file *pf, uint64_t i_id)
{
    (void)pf; (void)i_id;
    return NULL;
}

/* write_embedded_message() asks for the item of attachment N; the attachment
 * i_id is N (1-based) */
pst_item *test_pst_parse_item(pst_file *pf, pst_desc_tree *d_ptr, pst_id2_tree *m_head)
{
    (void)pf; (void)d_ptr; (void)m_head;
    int i = parsed++;
    if (i >= N_EMBEDDED) abort();

    /* by now every earlier embedded item has been freed */
    for (int j = 0; j < i; j++) {
        if (points_into(extra_mime_headers_under_test, j)) {
            fprintf(stderr, "embedded message %d: extra_mime_headers points into embedded message %d, which was freed\n", i+1, j+1);
            fail("extra_mime_headers outlived the embedded message it points into");
        }
    }

    pst_item *item = new_email(100 + i, i ? "second embedded message" : "first embedded message", "embedded body\n");
    set_str(&item->email->header, embedded_headers[i]);
    header_start[i] = (uintptr_t)item->email->header.str;
    header_end[i]   = header_start[i] + strlen(item->email->header.str);
    return item;
}

int main(void)
{
    char   *out  = NULL;
    size_t  size = 0;
    FILE   *f    = open_memstream(&out, &size);
    if (!f) { perror("open_memstream"); return 99; }

    mode = MODE_NORMAL;

    /* outer message: no transport headers, two embedded messages */
    pst_item *outer = new_email(1, "outer message", "outer body\n");
    pst_item_attach **tail = &outer->attach;
    for (int i = 0; i < N_EMBEDDED; i++) {
        pst_item_attach *a = calloc(1, sizeof(pst_item_attach));
        if (!a) abort();
        a->method = PST_ATTACH_EMBEDDED;
        a->i_id   = i + 1;
        *tail = a;
        tail  = &a->next;
    }

    pst_file pf;
    memset(&pf, 0, sizeof(pf));
    write_normal_email(f, "", outer, MODE_NORMAL, 0, &pf, 0, 0, &extra_mime_headers_under_test);
    fclose(f);

    if (parsed != N_EMBEDDED) fail("not every embedded message was written");
    if (extra_mime_headers_under_test) {
        for (int j = 0; j < N_EMBEDDED; j++) {
            if (points_into(extra_mime_headers_under_test, j))
                fprintf(stderr, "after the outer message: extra_mime_headers points into freed embedded message %d\n", j+1);
        }
        fail("extra_mime_headers is not NULL after the outer message was written");
    }
    if (!strstr(out, "Subject: first embedded message"))  fail("first embedded message missing from output");
    if (!strstr(out, "Subject: second embedded message")) fail("second embedded message missing from output");
    if (!strstr(out, "\n----boundary-LibPST-iamunique-1_-_---\n")) fail("outer multipart is not closed");

    if (failures) {
        fprintf(stderr, "--- output ---\n%s--- end of output ---\n", out);
    }
    else {
        printf("PASS: outer message without headers, %d embedded messages\n", N_EMBEDDED);
    }
    free(out);
    pst_freeItem(outer);
    return failures ? 1 : 0;
}
