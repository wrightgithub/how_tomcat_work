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


import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.ResourceBundle;

import org.apache.struts.Globals;
import org.apache.struts.action.ActionServlet;
import org.apache.struts.util.MessageResources;


/**
 * Class to hold the Locales supported by this package.
 *
 * @author Patrick Luby
 * @author Craig R. McClanahan
 * @version $Revision: 466595 $ $Date: 2006-10-21 23:24:41 +0100 (Sat, 21 Oct 2006) $
 */

public final class ApplicationLocales {


    // ----------------------------------------------------------- Constructors


    /**
     * Initialize the set of Locales supported by this application.
     *
     * @param servlet ActionServlet we are associated with
     */
    public ApplicationLocales(ActionServlet servlet) {

        super();
        Locale list[] = Locale.getAvailableLocales();
        MessageResources resources = (MessageResources)
            servlet.getServletContext().getAttribute(Globals.MESSAGES_KEY);
        if (resources == null)
            return;
        String config = resources.getConfig();
        if (config == null)
            return;

        for (int i = 0; i < list.length; i++) {
            ResourceBundle bundle =
                ResourceBundle.getBundle(config, list[i]);
            if (bundle == null)
                continue;
            if (list[i].equals(bundle.getLocale())) {
                localeLabels.add(list[i].getDisplayName());
                localeValues.add(list[i].toString());
                supportedLocales.add(list[i]);
            }
        }

    }


    // ----------------------------------------------------- Instance Variables


    /**
     * The set of Locale labels supported by this application.
     */
    protected ArrayList localeLabels = new ArrayList();


    /**
     * The set of Locale values supported by this application.
     */
    protected ArrayList localeValues = new ArrayList();


    /**
     * The set of supported Locales for this application.
     */
    protected ArrayList supportedLocales = new ArrayList();


    // --------------------------------------------------------- Public Methods


    /**
     * Return the set of Locale labels supported by this application.
     */
    public List getLocaleLabels() {

        return (localeLabels);

    }


    /**
     * Return the set of Locale values supported by this application.
     */
    public List getLocaleValues() {

        return (localeValues);

    }


    /**
     * Return the set of Locales supported by this application.
     */
    public List getSupportedLocales() {

        return (supportedLocales);

    }


}
