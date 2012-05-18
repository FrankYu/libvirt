/*
 * virsh-edit.c: Implementation of generic virsh *-edit intelligence
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * See COPYING.LIB for the License of this software.
 *
 * Usage:
 * Define macros:
 * EDIT_GET_XML - expression which produces a pointer to XML string, e.g:
 *      #define EDIT_GET_XML virDomainGetXMLDesc(dom, flags)
 *
 * EDIT_NOT_CHANGED - this action is taken if the XML wasn't changed.
 *      Note, that you don't want to jump to cleanup but edit_cleanup label
 *      where temporary variables are free()-d and temporary file is deleted:
 *      #define EDIT_NOT_CHANGED vshPrint(ctl, _("Domain %s XML not changed"), \
 *                                        virDomainGetName(dom)); \
 *                               ret = true; goto edit_cleanup;
 *      Note that this is a statement.
 *
 * EDIT_DEFINE - expression which redefines the object. The edited XML from
 *      user is in 'doc_edited' variable. Don't overwrite the pointer to the
 *      object, as we may iterate once more over and therefore the pointer
 *      would be invalid. Hence assign object to a different variable.
 *      Moreover, this needs to be an expression where:
 *      - 0 is taken as error (our virDefine* APIs often return NULL on error)
 *      - everything else is taken as success
 *      For example:
 *      #define EDIT_DEFINE (dom_edited = virDomainDefineXML(ctl->conn, doc_edited))
 *
 * EDIT_FREE - statement which vir*Free()-s object defined by EDIT_DEFINE, e.g:
 *      #define EDIT_FREE if (dom_edited) virDomainFree(dom_edited);
 *
 * Michal Privoznik <mprivozn@redhat.com>
 */

#ifndef EDIT_GET_XML
# error Missing EDIT_GET_XML definition
#endif

#ifndef EDIT_NOT_CHANGED
# error Missing EDIT_NOT_CHANGED definition
#endif

#ifndef EDIT_DEFINE
# error Missing EDIT_DEFINE definition
#endif

#ifndef EDIT_FREE
# error Missing EDIT_FREE definition
#endif

do {
    char *tmp = NULL;
    char *doc = NULL;
    char *doc_edited = NULL;
    char *doc_reread = NULL;
    char *msg = NULL;

    /* Get the XML configuration of the object. */
    doc = (EDIT_GET_XML);
    if (!doc)
        goto edit_cleanup;

    /* Create and open the temporary file. */
    tmp = editWriteToTempFile(ctl, doc);
    if (!tmp)
        goto edit_cleanup;

reedit:
    /* Start the editor. */
    if (editFile(ctl, tmp) == -1)
        goto edit_cleanup;

    /* Read back the edited file. */
    doc_edited = editReadBackFile(ctl, tmp);
    if (!doc_edited)
        goto edit_cleanup;

    /* Compare original XML with edited.  Has it changed at all? */
    if (STREQ(doc, doc_edited)) {
        EDIT_NOT_CHANGED;
    }

redefine:
    msg = NULL;

    /* Now re-read the object XML.  Did someone else change it while
     * it was being edited?  This also catches problems such as us
     * losing a connection or the object going away.
     */
    doc_reread = (EDIT_GET_XML);
    if (!doc_reread)
        goto edit_cleanup;

    if (STRNEQ(doc, doc_reread)) {
        msg = _("The XML configuration was changed by another user.");
        VIR_FREE(doc);
        doc = doc_reread;
        doc_reread = NULL;
    }

    /* Everything checks out, so redefine the object. */
    EDIT_FREE;
    if (!msg && !(EDIT_DEFINE)) {
        msg = _("Failed.");
    }

    if (msg) {
        int c = vshAskReedit(ctl, msg);
        switch (c) {
        case 'y':
            goto reedit;
            break;

        case 'f':
            goto redefine;
            break;

        case 'n':
            goto edit_cleanup;
            break;

        default:
            vshError(ctl, "%s", msg);
            break;
        }
    }

    break;

edit_cleanup:
    VIR_FREE(doc);
    VIR_FREE(doc_edited);
    VIR_FREE(doc_reread);
    if (tmp) {
        unlink (tmp);
        VIR_FREE(tmp);
    }
    goto cleanup;

} while (0);


#undef EDIT_GET_XML
#undef EDIT_NOT_CHANGED
#undef EDIT_DEFINE
#undef EDIT_FREE
