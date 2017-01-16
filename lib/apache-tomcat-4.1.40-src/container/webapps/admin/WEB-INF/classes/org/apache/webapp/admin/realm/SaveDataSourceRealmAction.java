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

package org.apache.webapp.admin.realm;

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
import org.apache.webapp.admin.logger.DeleteLoggerAction;

/**
 * The <code>Action</code> that completes <em>Add Realm</em> and
 * <em>Edit Realm</em> transactions for DataSource realm.
 * 
 * @author Amy Roh
 * @version $Revision: 1.1 $ $Date: 2004/04/17 02:53:17 $
 */

public final class SaveDataSourceRealmAction extends Action {

    // ----------------------------------------------------- Instance Variables

    /**
     * Signature for the <code>createDataSourceRealm</code> operation.
     */
    private String createDataSourceRealmTypes[] = { "java.lang.String", // parent
            "java.lang.String", // dataSourceName
            "java.lang.String", // roleNameCol
            "java.lang.String", // userCredCol
            "java.lang.String", // userNameCol
            "java.lang.String", // userRoleTable
            "java.lang.String", // userTable
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
     * @param mapping
     *            The ActionMapping used to select this instance
     * @param actionForm
     *            The optional ActionForm bean for this request (if any)
     * @param request
     *            The HTTP request we are processing
     * @param response
     *            The HTTP response we are creating
     * 
     * @exception IOException
     *                if an input/output error occurs
     * @exception ServletException
     *                if a servlet exception occurs
     */
    public ActionForward execute(ActionMapping mapping, ActionForm form,
            HttpServletRequest request, HttpServletResponse response)
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
            throw new ServletException("Cannot acquire MBeanServer reference",
                    t);
        }

        // Identify the requested action
        DataSourceRealmForm rform = (DataSourceRealmForm) form;
        String adminAction = rform.getAdminAction();
        String rObjectName = rform.getObjectName();

        // Perform a "Create DataSource Realm" transaction (if requested)
        if ("Create".equals(adminAction)) {

            String operation = null;
            String values[] = null;

            try {

                String parent = rform.getParentObjectName();
                String objectName = DeleteLoggerAction.getObjectName(parent,
                        TomcatTreeBuilder.REALM_TYPE);

                ObjectName pname = new ObjectName(parent);
                StringBuffer sb = new StringBuffer(pname.getDomain());

                // For service, create the corresponding Engine mBean
                // Parent in this case needs to be the container mBean for the
                // service
                try {
                    if ("Service"
                            .equalsIgnoreCase(pname.getKeyProperty("type"))) {
                        sb.append(":type=Engine");
                        parent = sb.toString();
                    }
                } catch (Exception e) {
                    String message = resources.getMessage(locale,
                            "error.engineName.bad", sb.toString());
                    getServlet().log(message);
                    response.sendError(HttpServletResponse.SC_BAD_REQUEST,
                            message);
                    return (null);
                }

                // Ensure that the requested user database name is unique
                ObjectName oname = new ObjectName(objectName);
                if (mBServer.isRegistered(oname)) {
                    ActionMessages errors = new ActionMessages();
                    errors.add("realmName", new ActionMessage(
                            "error.realmName.exists"));
                    saveErrors(request, errors);
                    return (new ActionForward(mapping.getInput()));
                }

                // Look up our MBeanFactory MBean
                ObjectName fname =
                    new ObjectName(TomcatTreeBuilder.FACTORY_TYPE);

                // Create a new DataSourceRealm object
                values = new String[7];
                values[0] = parent;
                values[1] = rform.getDataSourceName();
                values[2] = rform.getRoleNameCol();
                values[3] = rform.getUserCredCol();
                values[4] = rform.getUserNameCol();
                values[5] = rform.getUserRoleTable();
                values[6] = rform.getUserTable();
                operation = "createDataSourceRealm";
                rObjectName = (String) mBServer.invoke(fname, operation,
                        values, createDataSourceRealmTypes);

                if (rObjectName == null) {
                    request.setAttribute("warning", "error.datasourcerealm");
                    return (mapping.findForward("Save Unsuccessful"));
                }

                // Add the new Realm to our tree control node
                TreeControl control = (TreeControl) session
                        .getAttribute("treeControlTest");
                if (control != null) {
                    TreeControlNode parentNode = control.findNode(rform
                            .getParentObjectName());
                    if (parentNode != null) {
                        String nodeLabel = rform.getNodeLabel();
                        String encodedName = URLEncoder.encode(rObjectName);
                        TreeControlNode childNode =
                            new TreeControlNode(rObjectName,
                                                "Realm.gif",
                                                nodeLabel,
                                                "EditRealm.do?select="
                                                + encodedName,
                                                "content",
                                                true);
                        parentNode.addChild(childNode);
                        // FIXME - force a redisplay
                    } else {
                        getServlet().log(
                                "Cannot find parent node '" + parent + "'");
                    }
                } else {
                    getServlet().log("Cannot find TreeControlNode!");
                }

            } catch (Exception e) {

                getServlet().log(
                        resources.getMessage(locale, "users.error.invoke",
                                operation), e);
                response.sendError(
                        HttpServletResponse.SC_INTERNAL_SERVER_ERROR, resources
                                .getMessage(locale, "users.error.invoke",
                                        operation));
                return (null);

            }

        }

        // Perform attribute updates as requested
        String attribute = null;
        try {

            ObjectName roname = new ObjectName(rObjectName);

            attribute = "debug";
            int debug = 0;
            try {
                debug = Integer.parseInt(rform.getDebugLvl());
            } catch (Throwable t) {
                debug = 0;
            }
            mBServer.setAttribute(roname, new Attribute("debug", new Integer(
                    debug)));

            attribute = "dataSourceName";
            mBServer.setAttribute(roname, new Attribute(attribute, rform
                    .getDataSourceName()));
            attribute = "digest";
            mBServer.setAttribute(roname, new Attribute("digest", rform
                    .getDigest()));

            attribute = "roleNameCol";
            mBServer.setAttribute(roname, new Attribute("roleNameCol", rform
                    .getRoleNameCol()));
            attribute = "userCredCol";
            mBServer.setAttribute(roname, new Attribute("userCredCol", rform
                    .getUserCredCol()));
            attribute = "userNameCol";
            mBServer.setAttribute(roname, new Attribute("userNameCol", rform
                    .getUserNameCol()));
            attribute = "userRoleTable";
            mBServer.setAttribute(roname, new Attribute("userRoleTable", rform
                    .getUserRoleTable()));
            attribute = "userTable";
            mBServer.setAttribute(roname, new Attribute("userTable", rform
                    .getUserTable()));
        } catch (Exception e) {

            getServlet().log(
                    resources.getMessage(locale, "users.error.attribute.set",
                            attribute), e);
            response.sendError(HttpServletResponse.SC_INTERNAL_SERVER_ERROR,
                    resources.getMessage(locale, "users.error.attribute.set",
                            attribute));
            return (null);
        }

        // Forward to the success reporting page
        session.removeAttribute(mapping.getAttribute());
        return (mapping.findForward("Save Successful"));

    }

}