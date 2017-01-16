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

package org.apache.webapp.admin.context;

import java.io.IOException;
import java.util.Locale;
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
 * The <code>Action</code> that sets up <em>Edit Context</em> transactions.
 *
 * @author Manveen Kaur
 * @version $Revision: 466595 $ $Date: 2006-10-21 23:24:41 +0100 (Sat, 21 Oct 2006) $
 */

public class EditContextAction extends Action {
    

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
        
        // Set up the object names of the MBeans we are manipulating
        // Context mBean
        ObjectName cname = null;
        // Loader mBean
        ObjectName lname = null;
        // Manager mBean 
        ObjectName mname = null;
        
        StringBuffer sb = null;
        try {
            cname = new ObjectName(request.getParameter("select"));
        } catch (Exception e) {
            String message =
                resources.getMessage("error.contextName.bad",
                                     request.getParameter("select"));
            getServlet().log(message);
            response.sendError(HttpServletResponse.SC_BAD_REQUEST, message);
            return (null);
        }
        
        // Get the corresponding loader
        try {
            sb = new StringBuffer(cname.getDomain());
            sb.append(":type=Loader");
            sb.append(",path=");
            sb.append(cname.getKeyProperty("path"));
            sb.append(",host=");
            sb.append(cname.getKeyProperty("host"));
            sb.append(",service=");
            sb.append(cname.getKeyProperty("service"));
            lname = new ObjectName(sb.toString());
         } catch (Exception e) {
            String message =
                resources.getMessage("error.managerName.bad",
                                 sb.toString());
            getServlet().log(message);
            response.sendError(HttpServletResponse.SC_BAD_REQUEST, message);
            return (null);
        }

        // Session manager properties
        // Get the corresponding Session Manager mBean
        try {
            sb = new StringBuffer(cname.getDomain());
            sb.append(":type=Manager");
            sb.append(",path=");
            sb.append(cname.getKeyProperty("path"));
            sb.append(",host=");
            sb.append(cname.getKeyProperty("host"));
            sb.append(",service=");
            sb.append(cname.getKeyProperty("service"));
            mname = new ObjectName(sb.toString());
        } catch (Exception e) {
            String message =
                resources.getMessage("error.managerName.bad",
                                 sb.toString());
            getServlet().log(message);
            response.sendError(HttpServletResponse.SC_BAD_REQUEST, message);
            return (null);
        }

        // Fill in the form values for display and editing
        ContextForm contextFm = new ContextForm();
        session.setAttribute("contextForm", contextFm);
        contextFm.setAdminAction("Edit");
        contextFm.setObjectName(cname.toString());
        contextFm.setLoaderObjectName(lname.toString());
        contextFm.setManagerObjectName(mname.toString());
        sb = new StringBuffer("Context (");
        sb.append(cname.getKeyProperty("path"));
        sb.append(")");
        contextFm.setNodeLabel(sb.toString());
        contextFm.setDebugLvlVals(Lists.getDebugLevels());
        contextFm.setBooleanVals(Lists.getBooleanValues());
       
        String attribute = null;
        try {

            // Copy scalar properties
            attribute = "debug";
            contextFm.setDebugLvl
                (((Integer) mBServer.getAttribute(cname, attribute)).toString());
            attribute = "path";
            contextFm.setPath
                ((String) mBServer.getAttribute(cname, attribute));
            attribute = "cookies";
            contextFm.setCookies
                (((Boolean) mBServer.getAttribute(cname, attribute)).toString());
            attribute = "crossContext";
            contextFm.setCrossContext
                (((Boolean) mBServer.getAttribute(cname, attribute)).toString());
            attribute = "docBase";
            contextFm.setDocBase
                ((String) mBServer.getAttribute(cname, attribute));
            attribute = "workDir";
            contextFm.setWorkDir
                ((String) mBServer.getAttribute(cname, attribute));
            attribute = "useNaming";
            contextFm.setUseNaming
                (((Boolean) mBServer.getAttribute(cname, attribute)).toString());
            attribute = "reloadable";
            contextFm.setReloadable
                (((Boolean) mBServer.getAttribute(cname, attribute)).toString());
            attribute = "swallowOutput";
            contextFm.setSwallowOutput
                (((Boolean) mBServer.getAttribute(cname, attribute)).toString());
            attribute = "override";
            contextFm.setOverride
                (((Boolean) mBServer.getAttribute(cname, attribute)).toString());
            attribute = "privileged";
            contextFm.setPrivileged
                (((Boolean) mBServer.getAttribute(cname, attribute)).toString());
            attribute = "antiJARLocking";
            contextFm.setAntiJarLocking
                (((Boolean) mBServer.getAttribute(cname, attribute)).toString());

            // loader properties
            attribute = "debug";
            contextFm.setLdrDebugLvl
                (((Integer) mBServer.getAttribute(lname, attribute)).toString());
            attribute = "checkInterval";
            contextFm.setLdrCheckInterval
                (((Integer) mBServer.getAttribute(lname, attribute)).toString());
            attribute = "reloadable";
            contextFm.setLdrReloadable
                (((Boolean) mBServer.getAttribute(lname, attribute)).toString());

            // manager properties
            attribute = "debug";
            contextFm.setMgrDebugLvl
                (((Integer) mBServer.getAttribute(mname, attribute)).toString());
            attribute = "entropy";
            contextFm.setMgrSessionIDInit
                ((String) mBServer.getAttribute(mname, attribute));
            attribute = "maxActiveSessions";
            contextFm.setMgrMaxSessions
                (((Integer) mBServer.getAttribute(mname, attribute)).toString());
            attribute = "checkInterval";
            contextFm.setMgrCheckInterval
                (((Integer) mBServer.getAttribute(mname, attribute)).toString());

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
        
        // Forward to the context display page
        return (mapping.findForward("Context"));
        
    }


}
