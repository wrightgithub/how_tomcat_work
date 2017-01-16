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

package org.apache.webapp.admin.connector;

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
 * The <code>Action</code> that sets up <em>Edit Connector</em> transactions.
 *
 * @author Manveen Kaur
 * @version $Revision: 466595 $ $Date: 2006-10-21 23:24:41 +0100 (Sat, 21 Oct 2006) $
 */

public class EditConnectorAction extends Action {
    

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
        ObjectName cname = null;
        StringBuffer sb = null;
        try {
            cname = new ObjectName(request.getParameter("select"));
        } catch (Exception e) {
            String message =
                resources.getMessage("error.connectorName.bad",
                                     request.getParameter("select"));
            getServlet().log(message);
            response.sendError(HttpServletResponse.SC_BAD_REQUEST, message);
            return (null);
        }

        // Fill in the form values for display and editing
        ConnectorForm connectorFm = new ConnectorForm();
        session.setAttribute("connectorForm", connectorFm);
        connectorFm.setAdminAction("Edit");
        connectorFm.setObjectName(cname.toString());
        sb = new StringBuffer("Connector (");
        sb.append(cname.getKeyProperty("port"));
        sb.append(")");
        connectorFm.setNodeLabel(sb.toString());
        connectorFm.setDebugLvlVals(Lists.getDebugLevels());               
        connectorFm.setBooleanVals(Lists.getBooleanValues());        
        connectorFm.setClientAuthVals(Lists.getClientAuthValues());
        connectorFm.setThreadPriorityVals(Lists.getThreadPriorityValues());
        
        String attribute = null;
        try {

            // Copy scalar properties
            // General properties
            attribute = "scheme";
            String scheme = (String) mBServer.getAttribute(cname, attribute);
            connectorFm.setScheme(scheme);

            attribute = "protocolHandlerClassName";
            String handlerClassName = 
                (String) mBServer.getAttribute(cname, attribute);
            int period = handlerClassName.lastIndexOf('.');
            String connType = handlerClassName.substring(period + 1);
            String connectorType = "HTTPS";
            if ("JkCoyoteHandler".equalsIgnoreCase(connType)) {
                connectorType = "AJP";
            } else if ("Http11Protocol".equalsIgnoreCase(connType) && 
                      ("http".equalsIgnoreCase(scheme))) {
                connectorType = "HTTP";
            }             
            connectorFm.setConnectorType(connectorType);            
            
            attribute = "acceptCount";
            connectorFm.setAcceptCountText
                (((Integer) mBServer.getAttribute(cname, attribute)).toString());            
            attribute = "connectionTimeout";
            connectorFm.setConnTimeOutText
                (((Integer) mBServer.getAttribute(cname, attribute)).toString());            
            attribute = "debug";
            connectorFm.setDebugLvl
                (((Integer) mBServer.getAttribute(cname, attribute)).toString());            
            attribute = "bufferSize";
            connectorFm.setBufferSizeText
                (((Integer) mBServer.getAttribute(cname, attribute)).toString());            
            attribute = "enableLookups";
            connectorFm.setEnableLookups
                (((Boolean) mBServer.getAttribute(cname, attribute)).toString());            
            attribute = "address";
            connectorFm.setAddress
                ((String) mBServer.getAttribute(cname, attribute));
            attribute = "URIEncoding";
            connectorFm.setURIEncodingText
                ((String) mBServer.getAttribute(cname, attribute));
            attribute = "useBodyEncodingForURI";
            connectorFm.setUseBodyEncodingForURIText
                (((Boolean) mBServer.getAttribute(cname, attribute)).toString());       
            attribute = "allowTrace";
            connectorFm.setAllowTraceText
                (((Boolean) mBServer.getAttribute(cname, attribute)).toString());       
            attribute = "compressableMimeType";
            connectorFm.setCompressableMimeType
                ((String) mBServer.getAttribute(cname, attribute));
            attribute = "compression";
            connectorFm.setCompression
                ((String) mBServer.getAttribute(cname, attribute));
            attribute = "connectionLinger";
            connectorFm.setConnLingerText
                (((Integer) mBServer.getAttribute(cname, attribute)).toString());            
            attribute = "disableUploadTimeout";
            connectorFm.setDisableUploadTimeout
                (((Boolean) mBServer.getAttribute(cname, attribute)).toString());       
            attribute = "maxHttpHeaderSize";
            connectorFm.setMaxHttpHeaderSizeText
                (((Integer) mBServer.getAttribute(cname, attribute)).toString());            
            attribute = "maxKeepAliveRequests";
            connectorFm.setMaxKeepAliveReqsText
                (((Integer) mBServer.getAttribute(cname, attribute)).toString());            
            attribute = "noCompressionUserAgents";
            connectorFm.setNoCompressionUA
                ((String) mBServer.getAttribute(cname, attribute));
            attribute = "restrictedUserAgents";
            connectorFm.setRestrictedUA
                ((String) mBServer.getAttribute(cname, attribute));
            attribute = "server";
            connectorFm.setServer
                ((String) mBServer.getAttribute(cname, attribute));
            attribute = "strategy";
            connectorFm.setStrategy
                ((String) mBServer.getAttribute(cname, attribute));
            attribute = "tcpNoDelay";
            connectorFm.setTcpNoDelay
                (((Boolean) mBServer.getAttribute(cname, attribute)).toString());       
          
            // Ports
            attribute = "port";
            connectorFm.setPortText
                (((Integer) mBServer.getAttribute(cname, attribute)).toString());            
            attribute = "redirectPort";
            connectorFm.setRedirectPortText
                (((Integer) mBServer.getAttribute(cname, attribute)).toString());            
            
            // Processors
            attribute = "minProcessors";
            connectorFm.setMinProcessorsText
                (((Integer) mBServer.getAttribute(cname, attribute)).toString());            
            attribute = "maxProcessors";
            connectorFm.setMaxProcessorsText
                (((Integer) mBServer.getAttribute(cname, attribute)).toString());            
            attribute = "maxSpareProcessors";
            connectorFm.setMaxSpareProcessorsText
                (((Integer) mBServer.getAttribute(cname, attribute)).toString());            
            
            if ("AJP".equalsIgnoreCase(connectorType)) {
                // Supported by AJP only
                attribute = "tomcatAuthentication";
                connectorFm.setTomcatAuthentication
                    (((Boolean) mBServer.getAttribute(cname, attribute)).toString());       
            } else {
                // Supported by HTTP and HTTPS only
                attribute = "proxyName";
                connectorFm.setProxyName
                    ((String) mBServer.getAttribute(cname, attribute));
                attribute = "proxyPort";
                connectorFm.setProxyPortText
                    (((Integer) mBServer.getAttribute(cname, attribute)).toString());            
                attribute = "socketBuffer";
                connectorFm.setSocketBufferText
                    (((Integer) mBServer.getAttribute(cname, attribute)).toString());            
                attribute = "threadPriority";
                connectorFm.setThreadPriorityText
                    (((Integer) mBServer.getAttribute(cname, attribute)).toString());
            }
            
            // Secure
            attribute = "secure";
            connectorFm.setSecure
                (((Boolean) mBServer.getAttribute(cname, attribute)).toString());            
            
            if ("HTTPS".equalsIgnoreCase(connectorType)) {
                // Initialize rest of variables. 
                // These are set only for SSL connectors.
                attribute = "clientAuth";
                connectorFm.setClientAuthentication
                    ((String) mBServer.getAttribute(cname, attribute));
                attribute = "keystoreFile";
                connectorFm.setKeyStoreFileName
                    ((String) mBServer.getAttribute(cname, attribute));
                attribute = "keystorePass";
                connectorFm.setKeyStorePassword
                    ((String) mBServer.getAttribute(cname, attribute));            
                attribute = "algorithm";
                connectorFm.setAlgorithm
                    ((String) mBServer.getAttribute(cname, attribute));            
                attribute = "ciphers";
                connectorFm.setCiphers
                    ((String) mBServer.getAttribute(cname, attribute));            
                attribute = "keystoreType";
                connectorFm.setKeyStoreType
                    ((String) mBServer.getAttribute(cname, attribute));            
                attribute = "sslProtocol";
                connectorFm.setSslProtocol
                    ((String) mBServer.getAttribute(cname, attribute));            
            }     
                
                        
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
        
        // Forward to the connector display page
        return (mapping.findForward("Connector"));
        
    }


}
