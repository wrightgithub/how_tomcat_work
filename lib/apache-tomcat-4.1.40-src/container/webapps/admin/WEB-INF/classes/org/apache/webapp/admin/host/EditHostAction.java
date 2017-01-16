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

package org.apache.webapp.admin.host;

import java.io.IOException;
import java.util.Locale;
import java.util.Arrays;
import java.util.ArrayList;
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

import javax.management.MBeanServer;
import javax.management.ObjectName;

import org.apache.webapp.admin.ApplicationServlet;
import org.apache.webapp.admin.Lists;

/**
 * The <code>Action</code> that sets up <em>Edit Host</em> transactions.
 *
 * @author Manveen Kaur
 * @version $Revision: 466595 $ $Date: 2006-10-21 23:24:41 +0100 (Sat, 21 Oct 2006) $
 */

public class EditHostAction extends Action {
    
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
        
        String adminHost = null;
        // Get the host name the admin app runs on
        // this host cannot be deleted from the admin tool
        try {
            adminHost = Lists.getAdminAppHost(
                                  mBServer, "Catalina" ,request);
        } catch (Exception e) {
            String message =
                resources.getMessage("error.hostName.bad",
                                        adminHost);
            getServlet().log(message);
            response.sendError(HttpServletResponse.SC_BAD_REQUEST, message);
            return (null);
        }
        request.setAttribute("adminAppHost", adminHost);
                
        // Set up the object names of the MBeans we are manipulating
        ObjectName hname = null;
        StringBuffer sb = null;
        try {
            hname = new ObjectName(request.getParameter("select"));
        } catch (Exception e) {
            String message =
                resources.getMessage("error.hostName.bad",
                                     request.getParameter("select"));
            getServlet().log(message);
            response.sendError(HttpServletResponse.SC_BAD_REQUEST, message);
            return (null);
        }
        
        // Fill in the form values for display and editing
        HostForm hostFm = new HostForm();
        session.setAttribute("hostForm", hostFm);
        hostFm.setAdminAction("Edit");
        hostFm.setObjectName(hname.toString());
        sb = new StringBuffer("Host (");
        sb.append(hname.getKeyProperty("host"));
        sb.append(")");
        hostFm.setNodeLabel(sb.toString());
        hostFm.setDebugLvlVals(Lists.getDebugLevels());
        hostFm.setBooleanVals(Lists.getBooleanValues());
        
        String attribute = null;
        try {

            // Copy scalar properties
            attribute = "name";
            hostFm.setHostName
                ((String) mBServer.getAttribute(hname, attribute));
            attribute = "debug";
            hostFm.setDebugLvl
                (((Integer) mBServer.getAttribute(hname, attribute)).toString());
            attribute = "appBase";
            hostFm.setAppBase
                ((String) mBServer.getAttribute(hname, attribute));
            attribute = "autoDeploy";
            hostFm.setAutoDeploy
                (((Boolean) mBServer.getAttribute(hname, attribute)).toString());
            attribute = "deployXML";
            hostFm.setDeployXML
                (((Boolean) mBServer.getAttribute(hname, attribute)).toString());
            attribute = "liveDeploy";
            hostFm.setLiveDeploy
                (((Boolean) mBServer.getAttribute(hname, attribute)).toString());
            attribute = "unpackWARs";
            hostFm.setUnpackWARs
                (((Boolean) mBServer.getAttribute(hname, attribute)).toString());

        } catch (Throwable t) {
            getServlet().log
                (resources.getMessage(locale, "users.error.attribute.get",
                                      attribute), t);
            response.sendError
                (HttpServletResponse.SC_INTERNAL_SERVER_ERROR,
                 resources.getMessage(locale, "users.error.attribute.get",
                                      attribute));
            return (null);
        }

        // retrieve all aliases
        String operation = null;
        try {
            operation = "findAliases";
            String aliases[] = 
                (String[]) mBServer.invoke(hname, operation, null, null);
            
            hostFm.setAliasVals(new ArrayList(Arrays.asList(aliases)));

        } catch (Throwable t) {
            getServlet().log
            (resources.getMessage(locale, "users.error.invoke",
                                  operation), t);
            response.sendError
                (HttpServletResponse.SC_INTERNAL_SERVER_ERROR,
                resources.getMessage(locale, "users.error.invoke",
                                     operation));
            return (null);            
        }
                
        // Forward to the host display page
        return (mapping.findForward("Host"));
        
    }
}
