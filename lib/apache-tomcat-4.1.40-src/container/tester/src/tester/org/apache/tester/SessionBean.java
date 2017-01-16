/*
 * Copyright 1999, 2000 ,2004 The Apache Software Foundation.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.apache.tester;


import java.io.Serializable;
import java.sql.Date;
import javax.servlet.http.HttpSessionActivationListener;
import javax.servlet.http.HttpSessionBindingEvent;
import javax.servlet.http.HttpSessionBindingListener;
import javax.servlet.http.HttpSessionEvent;


/**
 * Simple JavaBean to use for session attribute tests.  It is Serializable
 * so that instances can be saved and restored across server restarts.
 *
 * @author Craig R. McClanahan
 * @version $Revision: 289023 $ $Date: 2004-08-26 23:06:34 +0100 (Thu, 26 Aug 2004) $
 */

public class SessionBean implements
    HttpSessionActivationListener, HttpSessionBindingListener, Serializable {


    // ------------------------------------------------------------- Properties


    /**
     * A date property for use with property editor tests.
     */
    protected Date dateProperty =
        new Date(System.currentTimeMillis());

    public Date getDateProperty() {
        return (this.dateProperty);
    }

    public void setDateProperty(Date dateProperty) {
        this.dateProperty = dateProperty;
    }


    /**
     * The lifecycle events that have happened on this bean instance.
     */
    protected String lifecycle = "";

    public String getLifecycle() {
        return (this.lifecycle);
    }

    public void setLifecycle(String lifecycle) {
        this.lifecycle = lifecycle;
    }


    /**
     * A string property.
     */
    protected String stringProperty = "Default String Property Value";

    public String getStringProperty() {
        return (this.stringProperty);
    }

    public void setStringProperty(String stringProperty) {
        this.stringProperty = stringProperty;
    }


    // --------------------------------------------------------- Public Methods


    /**
     * Return a string representation of this bean.
     */
    public String toString() {

        StringBuffer sb = new StringBuffer("SessionBean[lifecycle=");
        sb.append(this.lifecycle);
        sb.append(",dateProperty=");
        sb.append(dateProperty);
        sb.append(",stringProperty=");
        sb.append(this.stringProperty);
        sb.append("]");
        return (sb.toString());

    }


    // ---------------------------------- HttpSessionActivationListener Methods


    /**
     * Receive notification that this session was activated.
     *
     * @param event The session event that has occurred
     */
    public void sessionDidActivate(HttpSessionEvent event) {

        lifecycle += "/sda";

    }


    /**
     * Receive notification that this session will be passivated.
     *
     * @param event The session event that has occurred
     */
    public void sessionWillPassivate(HttpSessionEvent event) {

        lifecycle += "/swp";

    }


    // ------------------------------------- HttpSessionBindingListener Methods


    /**
     * Receive notification that this attribute has been bound.
     *
     * @param event The session event that has occurred
     */
    public void valueBound(HttpSessionBindingEvent event) {

        lifecycle += "/vb";

    }


    /**
     * Receive notification that this attribute has been unbound.
     *
     * @param event The session event that has occurred
     */
    public void valueUnbound(HttpSessionBindingEvent event) {

        lifecycle += "/vu";

    }


}

