/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.apache.webapp.admin.valve;

import java.util.Locale;
import java.io.IOException;
import javax.management.Attribute;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;

import org.apache.struts.Globals;
import org.apache.struts.action.Action;
import org.apache.struts.action.ActionForm;
import org.apache.struts.action.ActionForward;
import org.apache.struts.action.ActionMapping;
import org.apache.struts.util.MessageResources;
import org.apache.webapp.admin.ApplicationServlet;

/**
 * The <code>Action</code> that completes <em>Add Valve</em> and
 * <em>Edit Valve</em> transactions for AccessLog valve.
 *
 * @author Manveen Kaur
 * @version $Revision: 466595 $ $Date: 2006-10-21 23:24:41 +0100 (Sat, 21 Oct 2006) $
 */

public final class SaveAccessLogValveAction extends Action {


    // ----------------------------------------------------- Instance Variables

    /**
     * The MBeanServer we will be interacting with.
     */
    private MBeanServer mBServer = null;
    
    /**
     * The MessageResources we will be retrieving messages from.
     */
    private MessageResources resources = null;
    
    // --------------------------------------------------------- Public Methods
    
    
    /**
     * Process the specified HTTP request, and create the corresponding HTTP
     * response (or forward to another web component that will create it).
     * Return an <code>ActionForward</code> instance describing where and how
     * control should be forwarded, or <code>null</code> if the response has
     * already been completed.
     *
     * @param mapping The ActionMapping used to select this instance
     * @param actionForm The optional ActionForm bean for this request (if any)
     * @param request The HTTP request we are processing
     * @param response The HTTP response we are creating
     *
     * @exception IOException if an input/output error occurs
     * @exception ServletException if a servlet exception occurs
     */
    public ActionForward execute(ActionMapping mapping,
                                 ActionForm form,
                                 HttpServletRequest request,
                                 HttpServletResponse response)
        throws IOException, ServletException {
        
        // Acquire the resources that we need
        HttpSession session = request.getSession();
        Locale locale = (Locale) session.getAttribute(Globals.LOCALE_KEY);
        if (resources == null) {
            resources = getResources(request);
        }
        
        // Acquire a reference to the MBeanServer containing our MBeans
        try {
            mBServer = ((ApplicationServlet) getServlet()).getServer();
        } catch (Throwable t) {
            throw new ServletException
            ("Cannot acquire MBeanServer reference", t);
        }
        
        // Identify the requested action
        AccessLogValveForm vform = (AccessLogValveForm) form;
        String adminAction = vform.getAdminAction();
        String vObjectName = vform.getObjectName();
        String parent = vform.getParentObjectName();
        String valveType = vform.getValveType();
        
        // Perform a "Create Valve" transaction (if requested)
        if ("Create".equals(adminAction)) {
        
            vObjectName = ValveUtil.createValve(parent, valveType, 
                                response, request, mapping, 
                                (ApplicationServlet) getServlet());
           
        }

        // Perform attribute updates as requested
        String attribute = null;
        try {
        
            ObjectName voname = new ObjectName(vObjectName);
            
            attribute = "debug";
            int debug = 0;
            try {
                debug = Integer.parseInt(vform.getDebugLvl());
            } catch (Throwable t) {
                debug = 0;
            }
            mBServer.setAttribute(voname,
                                  new Attribute("debug", new Integer(debug)));
  
            attribute = "directory";
            mBServer.setAttribute(voname,
                         new Attribute("directory", vform.getDirectory()));
            attribute = "pattern";
            mBServer.setAttribute(voname,
                        new Attribute("pattern", vform.getPattern()));
            attribute = "prefix";
            mBServer.setAttribute(voname,
                        new Attribute("prefix", vform.getPrefix()));
            attribute = "suffix";
            mBServer.setAttribute(voname,
                        new Attribute("suffix", vform.getSuffix()));
            attribute = "resolveHosts";
            mBServer.setAttribute(voname,
                        new Attribute("resolveHosts", new Boolean(vform.getResolveHosts())));
            attribute = "rotatable";
            mBServer.setAttribute(voname,
                        new Attribute("rotatable", new Boolean(vform.getRotatable())));

        } catch (Exception e) {

            getServlet().log
                (resources.getMessage(locale, "users.error.attribute.set",
                                      attribute), e);
            response.sendError
                (HttpServletResponse.SC_INTERNAL_SERVER_ERROR,
                 resources.getMessage(locale, "users.error.attribute.set",
                                      attribute));
            return (null);
        }
    
        // Forward to the success reporting page
        session.removeAttribute(mapping.getAttribute());
        return (mapping.findForward("Save Successful"));
    }
}
