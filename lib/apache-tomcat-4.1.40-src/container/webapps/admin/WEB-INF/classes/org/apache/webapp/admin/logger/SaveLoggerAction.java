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

package org.apache.webapp.admin.logger;


import java.net.URLEncoder;
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
import org.apache.struts.action.ActionMessage;
import org.apache.struts.action.ActionMessages;
import org.apache.struts.action.ActionForm;
import org.apache.struts.action.ActionForward;
import org.apache.struts.action.ActionMapping;
import org.apache.struts.util.MessageResources;
import org.apache.webapp.admin.ApplicationServlet;
import org.apache.webapp.admin.TomcatTreeBuilder;
import org.apache.webapp.admin.TreeControl;
import org.apache.webapp.admin.TreeControlNode;



/**
 * The <code>Action</code> that completes <em>Add Logger</em> and
 * <em>Edit Logger</em> transactions.
 *
 * @author Manveen Kaur
 * @version $Revision: 466595 $ $Date: 2006-10-21 23:24:41 +0100 (Sat, 21 Oct 2006) $
 */

public final class SaveLoggerAction extends Action {


    // ----------------------------------------------------- Instance Variables

    /**
     * Signature for the <code>createStandardLogger</code> operation.
     */
    private String createStandardLoggerTypes[] =
    { "java.lang.String"     // parent
    };


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
        LoggerForm lform = (LoggerForm) form;
        String adminAction = lform.getAdminAction();
        String lObjectName = lform.getObjectName();
        String loggerType = lform.getLoggerType();

        // Perform a "Create Logger" transaction (if requested)
        if ("Create".equals(adminAction)) {

            String operation = null;
            String values[] = null;

            try {
   
                String parent = lform.getParentObjectName();                
                String objectName = DeleteLoggerAction.getObjectName(
                                        parent, TomcatTreeBuilder.LOGGER_TYPE);
                
                ObjectName pname = new ObjectName(parent);
                StringBuffer sb = new StringBuffer(pname.getDomain());                    
                
                // For service, create the corresponding Engine mBean  
                // Parent in this case needs to be the container mBean for the service 
                try {                                                        
                    if ("Service".equalsIgnoreCase(pname.getKeyProperty("type"))) {
                        sb.append(":type=Engine,service=");
                        sb.append(pname.getKeyProperty("name"));
                        parent = sb.toString();
                    }
                } catch (Exception e) {
                    String message =
                        resources.getMessage("error.engineName.bad",
                                         sb.toString());
                    getServlet().log(message);
                    response.sendError(HttpServletResponse.SC_BAD_REQUEST, message);
                    return (null);
                }
                                                
                // Ensure that the requested logger name is unique
                ObjectName oname =
                    new ObjectName(objectName);
                if (mBServer.isRegistered(oname)) {
                    ActionMessages errors = new ActionMessages();
                    errors.add("loggerName",
                               new ActionMessage("error.loggerName.exists"));
                    saveErrors(request, errors);
                    return (new ActionForward(mapping.getInput()));
                }

                // Look up our MBeanFactory MBean
                ObjectName fname =
                    new ObjectName(TomcatTreeBuilder.FACTORY_TYPE);

                // Create a new StandardLogger object
                values = new String[1];
                values[0] = parent;
                operation = "create" + loggerType;
                lObjectName = (String)
                    mBServer.invoke(fname, operation,
                                    values, createStandardLoggerTypes);
                // Add the new Logger to our tree control node
                TreeControl control = (TreeControl)
                    session.getAttribute("treeControlTest");
                if (control != null) {
                    TreeControlNode parentNode = control.findNode(lform.getParentObjectName());
                    if (parentNode != null) {
                        String nodeLabel =
                           "Logger for " + parentNode.getLabel();
                        String encodedName =
                            URLEncoder.encode(lObjectName);
                        TreeControlNode childNode =
                            new TreeControlNode(lObjectName,
                                                "Logger.gif",
                                                nodeLabel,
                                                "EditLogger.do?select=" +
                                                encodedName,
                                                "content",
                                                true);
                        parentNode.addChild(childNode);
                        // FIXME - force a redisplay
                    } else {
                        getServlet().log
                            ("Cannot find parent node '" + parent + "'");
                    }
                } else {
                    getServlet().log
                        ("Cannot find TreeControlNode!");
                }

            } catch (Exception e) {

                getServlet().log
                    (resources.getMessage(locale, "users.error.invoke",
                                          operation), e);
                response.sendError
                    (HttpServletResponse.SC_INTERNAL_SERVER_ERROR,
                     resources.getMessage(locale, "users.error.invoke",
                                          operation));
                return (null);

            }

        }

        // Perform attribute updates as requested
        String attribute = null;
        try {

            ObjectName loname = new ObjectName(lObjectName);

            attribute = "debug";
            int debug = 0;
            try {
                debug = Integer.parseInt(lform.getDebugLvl());
            } catch (Throwable t) {
                debug = 0;
            }
            mBServer.setAttribute(loname,
                                  new Attribute("debug", new Integer(debug)));
            attribute = "verbosity";
            int verbosity = 0;
            try {
                verbosity = Integer.parseInt(lform.getVerbosityLvl());
            } catch (Throwable t) {
                verbosity = 0;
            }
            mBServer.setAttribute(loname,
                                  new Attribute("verbosity", new Integer(verbosity)));            
            if("FileLogger".equalsIgnoreCase(loggerType)) {
                attribute = "directory";              
                mBServer.setAttribute(loname,
                                  new Attribute("directory", lform.getDirectory()));            
                attribute = "prefix";              
                mBServer.setAttribute(loname,
                                  new Attribute("prefix", lform.getPrefix()));            
                attribute = "suffix";              
                mBServer.setAttribute(loname,
                                  new Attribute("suffix", lform.getSuffix()));            
                attribute = "timestamp";              
                mBServer.setAttribute(loname,
                       new Attribute("timestamp", new Boolean(lform.getTimestamp())));;                            
            }
            
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
