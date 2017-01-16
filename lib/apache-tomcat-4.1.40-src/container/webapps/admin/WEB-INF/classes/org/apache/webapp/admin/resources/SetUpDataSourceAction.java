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

package org.apache.webapp.admin.resources;

import java.io.IOException;
import java.util.Locale;
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
 * <p>Implementation of <strong>Action</strong> that sets up and stashes
 * a <code>DataSourceForm</code> bean in request scope.  The form bean will have
 * a null <code>objectName</code> property if this form represents a DataSource
 * being added, or a non-null value for an existing DataSource.</p>
 *
 * @author Manveen Kaur
 * @version $Revision: 466595 $ $Date: 2006-10-21 23:24:41 +0100 (Sat, 21 Oct 2006) $
 * @since 4.1
 */

public final class SetUpDataSourceAction extends Action {

    // ----------------------------------------------------- Instance Variables


    /**
     * The MBeanServer we will be interacting with.
     */
    private MBeanServer mserver = null;


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

        // Look up the components we will be using as needed
        if (mserver == null) {
            mserver = ((ApplicationServlet) getServlet()).getServer();
        }
        if (resources == null) {
            resources = getResources(request);
        }
        HttpSession session = request.getSession();
        Locale locale = (Locale) session.getAttribute(Globals.LOCALE_KEY);

        // Set up the form bean based on the creating or editing state
        String objectName = request.getParameter("objectName");
        String resourcetype = request.getParameter("resourcetype");
        String path = request.getParameter("path");
        String host = request.getParameter("host");
        String service = request.getParameter("service");

        DataSourceForm dataSourceForm = new DataSourceForm();
        dataSourceForm.setResourcetype(resourcetype);
        dataSourceForm.setPath(path);
        dataSourceForm.setHost(host);
        dataSourceForm.setService(service);
        dataSourceForm.setType(ResourceUtils.DATASOURCE_CLASS);

        if (objectName == null) {
            dataSourceForm.setNodeLabel
                (resources.getMessage(locale, "resources.actions.datasrc.create"));
            dataSourceForm.setObjectName(null);
            dataSourceForm.setActive("4");
            dataSourceForm.setIdle("2");
            dataSourceForm.setWait("5000");
            dataSourceForm.setType(ResourceUtils.DATASOURCE_CLASS);

        } else {
            dataSourceForm.setNodeLabel
                (resources.getMessage(locale, "resources.actions.datasrc.edit"));
            dataSourceForm.setObjectName(objectName);

            String attribute = null;
            try {
                ObjectName oname = new ObjectName(objectName);
                attribute = "name";
                dataSourceForm.setJndiName
                    ((String) mserver.getAttribute(oname, attribute));
                attribute = "url";
                dataSourceForm.setUrl
                    ((String) mserver.getAttribute(oname, attribute));
                attribute = "driverClassName";
                dataSourceForm.setDriverClass
                    ((String) mserver.getAttribute(oname, attribute));
                attribute = "username";
                dataSourceForm.setUsername
                    ((String) mserver.getAttribute(oname, attribute));
                attribute = "password";
                dataSourceForm.setPassword
                    ((String) mserver.getAttribute(oname, attribute));
                try {
                    attribute = "maxActive";
                    dataSourceForm.setActive
                        ((String) mserver.getAttribute(oname, attribute));
                } catch (Exception e) {
                    // if maxActive not defined, display default value
                    dataSourceForm.setActive("4");
                }
                try {
                    attribute = "maxIdle";
                    dataSourceForm.setIdle
                        ((String) mserver.getAttribute(oname, attribute));
                } catch (Exception e) {
                    // if maxIdle not defined, display default value
                    dataSourceForm.setIdle("2");
                }
                try {
                    attribute = "maxWait";
                    dataSourceForm.setWait
                        ((String) mserver.getAttribute(oname, attribute));
                } catch (Exception e) {
                    // if maxWait not defined, display default value
                    dataSourceForm.setWait("5000");
                }
                try {
                    attribute = "validationQuery";
                    dataSourceForm.setQuery
                        ((String) mserver.getAttribute(oname, attribute));
                } catch (Exception e) {
                    // don't display anything
                }
            } catch (Exception e) {
                getServlet().log
                    (resources.getMessage(locale,
                        "users.error.attribute.get", attribute), e);
                response.sendError
                    (HttpServletResponse.SC_INTERNAL_SERVER_ERROR,
                     resources.getMessage
                         (locale, "users.error.attribute.get", attribute));
                return (null);
            }
        }

        // Stash the form bean and forward to the display page
        saveToken(request);
        request.setAttribute("dataSourceForm", dataSourceForm);
        return (mapping.findForward("DataSource"));

    }
}
