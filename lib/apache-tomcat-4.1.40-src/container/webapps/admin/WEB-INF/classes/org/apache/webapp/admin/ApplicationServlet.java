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


package org.apache.webapp.admin;

import javax.management.MBeanServer;
import javax.servlet.ServletException;
import javax.servlet.UnavailableException;
import org.apache.commons.modeler.Registry;
import org.apache.struts.action.ActionServlet;


/**
 * Subclass of ActionServlet that adds caching of the supported locales in the
 * ApplicationLocales class.
 *
 * @author Patrick Luby
 * @version $Revision: 466595 $ $Date: 2006-10-21 23:24:41 +0100 (Sat, 21 Oct 2006) $
 */

public class ApplicationServlet extends ActionServlet {


    // ----------------------------------------------------- Manifest Constants


    /**
     * The application scope key under which we store our
     * <code>ApplicationLocales</code> instance.
     */
    public static final String LOCALES_KEY = "applicationLocales";


    // ----------------------------------------------------- Instance Variables


    /**
     * The managed beans Registry used to look up metadata.
     */
    protected Registry registry = null;


    /**
     * The JMX MBeanServer we will use to look up management beans.
     */
    protected MBeanServer server = null;


    // --------------------------------------------------------- Public Methods


    /**
     * Convenience method to make the managed beans Registry available.
     *
     * @exception ServletException if the Registry is not available
     */
    public Registry getRegistry() throws ServletException {

        if (registry == null)
            initRegistry();
        return (this.registry);

    }


    /**
     * Convenience method to make the JMX MBeanServer available.
     *
     * @exception ServletException if the MBeanServer is not available
     */
    public MBeanServer getServer() throws ServletException {

        if (server == null)
            initServer();
        return (this.server);

    }


    /**
     * Initialize this servlet.
     *
     * @exception ServletException if an initialization error occurs.
     */
    public void init() throws javax.servlet.ServletException {

        // Perform normal superclass initialization
        super.init();

        // Perform initialization specific to this application
        initApplicationLocales();

    }


    // ---------------------------------------------------- Protected Methods


    /**
     * Create and initialize the ApplicationLocales object, and make it
     * available as a servlet context attribute.
     */
    protected void initApplicationLocales() {

        ApplicationLocales locales = new ApplicationLocales(this);
        getServletContext().setAttribute(LOCALES_KEY, locales);

    }


    /**
     * Validate the existence of the Registry that should have been
     * provided to us by an instance of
     * <code>org.apache.catalina.mbean.ServerLifecycleListener</code>
     * enabled at startup time.
     *
     * @exception ServletException if we cannot find the Registry
     */
    protected void initRegistry() throws ServletException {

        registry = (Registry) getServletContext().getAttribute
            ("org.apache.catalina.Registry");
        if (registry == null)
            throw new UnavailableException("Registry is not available");

    }


    /**
     * Validate the existence of the MBeanServer that should have been
     * provided to us by an instance of
     * <code>org.apache.catalina.mbean.ServerLifecycleListener</code>
     * enabled at startup time.
     *
     * @exception ServletException if we cannot find the MBeanServer
     */
    protected void initServer() throws ServletException {

        server = (MBeanServer) getServletContext().getAttribute
            ("org.apache.catalina.MBeanServer");
        if (server == null)
            throw new UnavailableException("MBeanServer is not available");

    }


}
