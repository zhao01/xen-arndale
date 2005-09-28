#============================================================================
# This library is free software; you can redistribute it and/or
# modify it under the terms of version 2.1 of the GNU Lesser General Public
# License as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#============================================================================
# Copyright (C) 2004, 2005 Mike Wray <mike.wray@hp.com>
#============================================================================

import traceback
from StringIO import StringIO

from xen.web import http

from xen.xend import sxp
from xen.xend import XendDomain
from xen.xend.Args import FormFn
from xen.xend.XendError import XendError
from xen.xend.XendLogging import log

from xen.web.SrvDir import SrvDir
from SrvDomain import SrvDomain

class SrvDomainDir(SrvDir):
    """Service that manages the domain directory.
    """

    def __init__(self):
        SrvDir.__init__(self)
        self.xd = XendDomain.instance()

    def domain(self, x):
        val = None
        dom = self.xd.domain_lookup_by_name(x)
        if not dom:
            raise XendError('No such domain ' + str(x))
        val = SrvDomain(dom)
        return val

    def get(self, x):
        v = SrvDir.get(self, x)
        if v is not None:
            return v
        v = self.domain(x)
        return v

    def op_create(self, op, req):
        """Create a domain.
        Expects the domain config in request parameter 'config' in SXP format.
        """
        ok = 0
        errmsg = ''
        try:
            configstring = req.args.get('config')[0]
            #print 'op_create>', 'config:', configstring
            pin = sxp.Parser()
            pin.input(configstring)
            pin.input_eof()
            config = pin.get_val()
            ok = 1
        except Exception, ex:
            print 'op_create> Exception in config', ex
            traceback.print_exc()
            errmsg = 'Configuration error ' + str(ex)
        except sxp.ParseError, ex:
            errmsg = 'Invalid configuration ' + str(ex)
        if not ok:
            raise XendError(errmsg)
        try:
            dominfo = self.xd.domain_create(config)
            return self._op_create_cb(dominfo, configstring, req)
        except Exception, ex:
            print 'op_create> Exception creating domain:'
            traceback.print_exc()
            raise XendError("Error creating domain: " + str(ex))

    def _op_create_cb(self, dominfo, configstring, req):
        """Callback to handle domain creation.
        """
        dom = dominfo.getName()
        domurl = "%s/%s" % (req.prePathURL(), dom)
        req.setResponseCode(http.CREATED, "created")
        req.setHeader("Location", domurl)
        if self.use_sxp(req):
            return dominfo.sxpr()
        else:
            out = StringIO()
            print >> out, ('<p> Created <a href="%s">Domain %s</a></p>'
                           % (domurl, dom))
            print >> out, '<p><pre>'
            print >> out, configstring
            print >> out, '</pre></p>'
            val = out.getvalue()
            out.close()
            return val

    def op_restore(self, op, req):
        """Restore a domain from file.

        """
        return req.threadRequest(self.do_restore, op, req)

    def do_restore(self, op, req):
        fn = FormFn(self.xd.domain_restore,
                    [['file', 'str']])
        dominfo = fn(req.args)
        dom = dominfo.getName()
        domurl = "%s/%s" % (req.prePathURL(), dom)
        req.setResponseCode(http.CREATED)
        req.setHeader("Location", domurl)
        if self.use_sxp(req):
            return dominfo.sxpr()
        else:
            out = StringIO()
            print >> out, ('<p> Created <a href="%s">Domain %s</a></p>'
                           % (domurl, dom))
            val = out.getvalue()
            out.close()
            return val

    def render_POST(self, req):
        return self.perform(req)

    def render_GET(self, req):
        if self.use_sxp(req):
            req.setHeader("Content-Type", sxp.mime_type)
            self.ls_domain(req, 1)
        else:
            req.write("<html><head></head><body>")
            self.print_path(req)
            self.ls(req)
            self.ls_domain(req)
            self.form(req)
            req.write("</body></html>")

    def ls_domain(self, req, use_sxp=0):
        url = req.prePathURL()
        if not url.endswith('/'):
            url += '/'
        if use_sxp:
            domains = self.xd.list_names()
            sxp.show(domains, out=req)
        else:
            domains = self.xd.list_sorted()
            req.write('<ul>')
            for d in domains:
                req.write(
                    '<li><a href="%s%s">Domain %s</a>: id = %s, memory = %d, '
                    'ssidref = %d.'
                    % (url, d.getName(), d.getName(), d.getDomid(),
                       d.getMemoryTarget(), d.getSsidref()))
                req.write('</li>')
            req.write('</ul>')

    def form(self, req):
        """Generate the form(s) for domain dir operations.
        """
        req.write('<form method="post" action="%s" enctype="multipart/form-data">'
                  % req.prePathURL())
        req.write('<button type="submit" name="op" value="create">Create Domain</button>')
        req.write('Config <input type="file" name="config"><br>')
        req.write('</form>')
        
        req.write('<form method="post" action="%s" enctype="multipart/form-data">'
                  % req.prePathURL())
        req.write('<button type="submit" name="op" value="restore">Restore Domain</button>')
        req.write('State <input type="string" name="state"><br>')
        req.write('</form>')
        
