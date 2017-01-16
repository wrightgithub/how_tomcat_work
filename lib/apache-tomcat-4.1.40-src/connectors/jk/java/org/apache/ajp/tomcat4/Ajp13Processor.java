/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

package org.apache.ajp.tomcat4;


import java.io.IOException;
import java.net.Socket;

import javax.servlet.ServletException;
import javax.servlet.http.HttpServletResponse;

import org.apache.ajp.Ajp13;
import org.apache.catalina.Lifecycle;
import org.apache.catalina.LifecycleException;
import org.apache.catalina.LifecycleListener;
import org.apache.catalina.util.LifecycleSupport;
import org.apache.catalina.util.StringManager;
import org.apache.tomcat.util.http.BaseRequest;

/**
 * @author Kevin Seguin
 * @version $Revision: 466585 $ $Date: 2006-10-21 23:16:34 +0100 (Sat, 21 Oct 2006) $
 */

final class Ajp13Processor
    implements Lifecycle, Runnable {

    /**
     * A simple class to provide synchronized access
     * to a boolean.
     */
    private class Bool {

        private boolean b = false;

        Bool() {
        }
        
        Bool(boolean b) {
            this.b = b;
        }

        synchronized boolean value() {
            return b;
        }

        synchronized void set(boolean b) {
            this.b = b;
        }
    }

    // ----------------------------------------------------------- Constructors


    /**
     * Construct a new Ajp13Processor associated with the specified connector.
     *
     * @param connector Ajp13Connector that owns this processor
     * @param id Identifier of this Ajp13Processor (unique per connector)
     * @param threadGroup The thread group any threads created by the processor
     *        should be in.
     */
    public Ajp13Processor(Ajp13Connector connector,
                          int id,
                          ThreadGroup threadGroup) {

	super();
	this.connector = connector;
	this.debug = connector.getDebug();
	this.id = id;
	this.request = (Ajp13Request) connector.createRequest();
        this.request.setConnector(connector);
        this.request.setConnector(connector);
	this.response = (Ajp13Response) connector.createResponse();
        this.response.setConnector(connector);
	this.threadName =
	  "Ajp13Processor[" + connector.getPort() + "][" + id + "]";
        this.threadGroup = threadGroup;

        this.logger.setConnector(connector);
        this.logger.setName(this.threadName);
    }


    // ----------------------------------------------------- Instance Variables

    private Ajp13Logger logger = new Ajp13Logger();
    private BaseRequest ajpRequest = new BaseRequest();

    /**
     * Is there a new socket available?
     */
    private boolean available = false;


    /**
     * The Ajp13Connector with which this processor is associated.
     */
    private Ajp13Connector connector = null;


    /**
     * The debugging detail level for this component.
     */
    private int debug = 0;


    /**
     * The identifier of this processor, unique per connector.
     */
    private int id = 0;


    /**
     * The lifecycle event support for this component.
     */
    private LifecycleSupport lifecycle = new LifecycleSupport(this);


    /**
     * The AJP13 request object we will pass to our associated container.
     */
    private Ajp13Request request = null;


    /**
     * The AJP13 response object we will pass to our associated container.
     */
    private Ajp13Response response = null;


    /**
     * The string manager for this package.
     */
    protected StringManager sm =
	StringManager.getManager(Constants.PACKAGE);


    /**
     * The socket we are currently processing a request for.  This object
     * is used for inter-thread communication only.
     */
    private Socket socket = null;


    /**
     * Has this component been started yet?
     */
    private boolean started = false;


    /**
     * The shutdown signal to our background thread
     */
    private Bool stopped = new Bool(true);

    /**
     * Are we currently handling a request?
     */
    private Bool handlingRequest = new Bool(false);


    /**
     * The background thread.
     */
    private Thread thread = null;


    /**
     * The name to register for the background thread.
     */
    private String threadName = null;


    /**
     * This processor's thread group.
     */
    private ThreadGroup threadGroup = null;


    /**
     * The thread synchronization object.
     */
    private Object threadSync = new Object();



    // -------------------------------------------------------- Package Methods


    /**
     * Process an incoming TCP/IP connection on the specified socket.  Any
     * exception that occurs during processing must be logged and swallowed.
     * <b>NOTE</b>:  This method is called from our Connector's thread.  We
     * must assign it to our own thread so that multiple simultaneous
     * requests can be handled.
     *
     * @param socket TCP socket to process
     */
    synchronized void assign(Socket socket) {

        // Wait for the Processor to get the previous Socket
        while (available) {
	    try {
	        wait();
	    } catch (InterruptedException e) {
	    }
        }

	// Store the newly available Socket and notify our thread
	this.socket = socket;
	available = true;
	notifyAll();

	if ((debug > 0) && (socket != null))
	    logger.log(" An incoming request is being assigned");

    }


    // -------------------------------------------------------- Private Methods


    /**
     * Await a newly assigned Socket from our Connector, or <code>null</code>
     * if we are supposed to shut down.
     */
    private synchronized Socket await() {

        // Wait for the Connector to provide a new Socket
        while (!available) {
	    try {
	        wait();
	    } catch (InterruptedException e) {
	    }
        }

	// Notify the Connector that we have received this Socket
	Socket socket = this.socket;
	available = false;
	notifyAll();

	if ((debug > 0) && (socket != null))
	    logger.log("  The incoming request has been awaited");

	return (socket);

    }

    /**
     * Parse and record the connection parameters related to this request.
     *
     * @param socket The socket on which we are connected
     *
     * @exception IOException if an input/output error occurs
     * @exception ServletException if a parsing error occurs
     */
    private void parseConnection(Socket socket)
        throws IOException, ServletException {

	if (debug > 1)
	    logger.log("  parseConnection: address=" + socket.getInetAddress() +
		", port=" + connector.getPort());
	request.setServerPort(connector.getPort());
        request.setSocket(socket);

    }

    /**
     * Process an incoming AJP13 request on the Socket that has been assigned
     * to this Processor.  Any exceptions that occur during processing must be
     * swallowed and dealt with.
     *
     * @param socket The socket on which we are connected to the client
     */
    private void process(Socket socket) {

        Ajp13 ajp13 = new Ajp13();
        ajp13.setDebug(debug);
        ajp13.setLogger(new org.apache.ajp.Logger() {
                public void log(String msg) {
                    logger.log("[Ajp13] " + msg);
                }
                
                public void log(String msg, Throwable t) {
                    logger.log("[Ajp13] " + msg, t);
                }
            });

        Ajp13InputStream input = new Ajp13InputStream(ajp13);
        Ajp13OutputStream output = new Ajp13OutputStream(ajp13);
        response.setAjp13(ajp13);

        try {
            ajp13.setSocket(socket);
        } catch (IOException e) {
            logger.log("process: ajp13.setSocket", e);
        }

        boolean moreRequests = true;
        String expectedSecret=connector.getSecret();
        
        boolean needAuth= ( expectedSecret != null );
        
        while (moreRequests && !stopped.value()) {
            
            int status = 0;
            try {
                if (debug > 0) {
                    logger.log("waiting on next request...");
                }
                
                status = ajp13.receiveNextRequest(ajpRequest);
                
                if (debug > 0) {
                    logger.log("received next request, status=" + status);
                }
            } catch (IOException e) {
                logger.log("process: ajp13.receiveNextRequest", e);
            }

            if( needAuth ) {
                String connSecret=ajp13.getSecret();
                if( connSecret == null ) {
                    logger.log( "Connection without password, " +
                                "tomcat is configured to require one" );
                    break;
                }
                if( ! connSecret.equals(expectedSecret) ) {
                    logger.log( "Connection with wrong password" );
                    break;
                }
                
                needAuth=false;
            }
            
            if (stopped.value()) {
                if (debug > 0) {
                    logger.log("process:  received request, but we're stopped");
                }
                break;
            }
            
            if( status==-2) {
                // special case - shutdown
                // XXX need better communication, refactor it
//                  if( !doShutdown(socket.getLocalAddress(),
//                                  socket.getInetAddress())) {
//                      moreRequests = false;
//                      continue;
//                  }
                break;
            }
            
			// Allready handled by low level proto, don't go farther
			if( status == 999 )
			{
				ajpRequest.recycle();
				request.recycle();

				// recycle ajp13 object
				ajp13.recycle();

				continue;
			}

			if( status != 200 )
				break;

            try {
                // set flag
                handlingRequest.set(true);

                boolean bad_request = false;

                // set up request
                try {
                    request.setAjpRequest(ajpRequest);
                } catch (IllegalArgumentException e) {
                    bad_request = true;
                }
                request.setResponse(response);
                request.setStream(input);
                
                // setup response
                response.setRequest(request);
                response.setStream(output);
                
                if (debug > 0) {
                    logger.log("invoking...");
                }

                if (!bad_request) {
                    try {
                        connector.getContainer().invoke(request, response);
                    } catch (IOException ioe) {
                        // Pass the IOException through
                        throw ioe;
                    } catch (Throwable e) {
                        // A throwable here could be caused by a Valve,
                        // Filter, or other component in the chain.
                        // Processing of the request failed, return an
                        // Internal Server Error
                        logger.log("process: invoke", e);
                        response.sendError
                            (HttpServletResponse.SC_INTERNAL_SERVER_ERROR);
                    }
                } else {
                    response.sendError
                        (HttpServletResponse.SC_BAD_REQUEST);
                }

                if (debug > 0) {
                    logger.log("done invoking, finishing request/response....");
                }

                response.finishResponse();
                request.finishRequest();

                if (debug > 0) {
                    logger.log("finished handling request.");
                }

            } catch (IOException ioe) {
                // Normally this catches a socket Broken Pipe caused by the
                // remote client aborting the request. Don't print the stack
                // trace in this case. Then let the Processor recycle.
                logger.log("process: IOException " + ioe.getMessage());
                moreRequests = false;
            } catch (Throwable e) {
                // Processing the request and sending the response failed.
                // We don't know what the state of the Ajp Connector socket
                // is in. Bail out and recycle the Processor.
                logger.log("process: finish", e);
                moreRequests = false;
            }

            // Recycling the request and the response objects
            if (debug > 0) {
                logger.log("recyling objects ...");
            }
            
            ajpRequest.recycle();
            request.recycle();
            response.recycle();

            // recycle ajp13 object
            ajp13.recycle();

            // reset flag
            handlingRequest.set(false);
        }
        
	try {
            if (debug > 0) {
                logger.log("closing ajp13 object...");
            }

            ajp13.close();

            if (debug > 0) {
                logger.log("ajp13 object closed.");
            }
	} catch (IOException e) {
	    logger.log("process: ajp13.close", e);
	}

	try {
            if (debug > 0) {
                logger.log("closing socket...");
            }

            socket.close();

            if (debug > 0) {
                logger.log("socket closed.");
            }
	} catch (IOException e) {
	    logger.log("process: socket.close", e);
	}
	socket = null;

        if (debug > 0) {
            logger.log("process:  done");
        }
    }


    // ---------------------------------------------- Background Thread Methods


    /**
     * The background thread that listens for incoming TCP/IP connections and
     * hands them off to an appropriate processor.
     */
    public void run() {

        // Process requests until we receive a shutdown signal
	while (!stopped.value()) {

	    // Wait for the next socket to be assigned
            if (debug > 0) {
                logger.log("waiting for next socket to be assigned...");
            }
	    Socket socket = await();
	    if (socket == null)
		continue;

            if (debug > 0) {
                logger.log("socket assigned.");
            }

	    // Process the request from this socket
	    process(socket);

	    // Finish up this request
            if (debug > 0) {
                logger.log("recycling myself ...");
            }
	    connector.recycle(this);
	}

	// Tell threadStop() we have shut ourselves down successfully
	synchronized (threadSync) {
	    threadSync.notifyAll();
	}

    }


    /**
     * Start the background processing thread.
     */
    private void threadStart() {

	logger.log(sm.getString("ajp13Processor.starting"));

        stopped.set(false);
	thread = new Thread(threadGroup, this, threadName);
	thread.setDaemon(true);
	thread.start();

	if (debug > 0)
	    logger.log(" Background thread has been started");

    }


    /**
     * Stop the background processing thread.
     */
    private void threadStop() {

	logger.log(sm.getString("ajp13Processor.stopping"));

	stopped.set(true);
        assign(null);
	synchronized (threadSync) {
	    try {
                if (handlingRequest.value()) {
                    if (debug > 0) {
                        logger.log
                            ("currentling handling a request, so waiting....");
                    }
                    threadSync.wait(5000);
                } else {
                    if (debug > 0) {
                        logger.log
                            ("not currently handling a request, not waiting.");
                    }
                }
	    } catch (InterruptedException e) {
		;
	    }
	}
	thread = null;

    }


    // ------------------------------------------------------ Lifecycle Methods


    /**
     * Add a lifecycle event listener to this component.
     *
     * @param listener The listener to add
     */
    public void addLifecycleListener(LifecycleListener listener) {

	lifecycle.addLifecycleListener(listener);

    }

    /**
     * Get the lifecycle listeners associated with this lifecycle. If this
     * Lifecycle has no listeners registered, a zero-length array is returned.
     */
    public LifecycleListener[] findLifecycleListeners() {
        return null; // FIXME: lifecycle.findLifecycleListeners();
    }


    /**
     * Remove a lifecycle event listener from this component.
     *
     * @param listener The listener to add
     */
    public void removeLifecycleListener(LifecycleListener listener) {

	lifecycle.removeLifecycleListener(listener);

    }


    /**
     * Start the background thread we will use for request processing.
     *
     * @exception LifecycleException if a fatal startup error occurs
     */
    public void start() throws LifecycleException {

	if (started)
	    throw new LifecycleException
		(sm.getString("ajp13Processor.alreadyStarted"));
	lifecycle.fireLifecycleEvent(START_EVENT, null);
	started = true;

	threadStart();

    }


    /**
     * Stop the background thread we will use for request processing.
     *
     * @exception LifecycleException if a fatal shutdown error occurs
     */
    public void stop() throws LifecycleException {

	if (!started)
	    throw new LifecycleException
		(sm.getString("ajp13Processor.notStarted"));
	lifecycle.fireLifecycleEvent(STOP_EVENT, null);
	started = false;

	threadStop();

    }


}
