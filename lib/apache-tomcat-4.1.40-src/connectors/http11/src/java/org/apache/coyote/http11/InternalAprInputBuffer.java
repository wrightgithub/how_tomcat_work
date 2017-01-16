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


package org.apache.coyote.http11;

import java.io.IOException;
import java.io.EOFException;
import java.net.SocketTimeoutException;
import java.nio.ByteBuffer;

import org.apache.tomcat.jni.Socket;
import org.apache.tomcat.jni.Status;
import org.apache.tomcat.util.buf.ByteChunk;
import org.apache.tomcat.util.buf.MessageBytes;
import org.apache.tomcat.util.http.MimeHeaders;
import org.apache.tomcat.util.res.StringManager;

import org.apache.coyote.InputBuffer;
import org.apache.coyote.Request;

/**
 * Implementation of InputBuffer which provides HTTP request header parsing as
 * well as transfer decoding.
 *
 * @author <a href="mailto:remm@apache.org">Remy Maucherat</a>
 */
public class InternalAprInputBuffer implements InputBuffer {


    // -------------------------------------------------------------- Constants


    // ----------------------------------------------------------- Constructors


    /**
     * Alternate constructor.
     */
    public InternalAprInputBuffer(Request request, int headerBufferSize, 
                                  long readTimeout) {

        this.request = request;
        headers = request.getMimeHeaders();

        headerBuffer1 = new byte[headerBufferSize];
        headerBuffer2 = new byte[headerBufferSize];
        bodyBuffer = new byte[headerBufferSize];
        buf = headerBuffer1;
        bbuf = ByteBuffer.allocateDirect(headerBufferSize);

        headerBuffer = new char[headerBufferSize];
        ascbuf = headerBuffer;

        inputStreamInputBuffer = new SocketInputBuffer();

        filterLibrary = new InputFilter[0];
        activeFilters = new InputFilter[0];
        lastActiveFilter = -1;

        parsingHeader = true;
        swallowInput = true;
        
        if (readTimeout < 0) {
            this.readTimeout = -1;
        } else {
            this.readTimeout = readTimeout * 1000;
        }

    }


    // -------------------------------------------------------------- Variables


    /**
     * The string manager for this package.
     */
    protected static StringManager sm =
        StringManager.getManager(Constants.Package);


    // ----------------------------------------------------- Instance Variables


    /**
     * Associated Coyote request.
     */
    protected Request request;


    /**
     * Headers of the associated request.
     */
    protected MimeHeaders headers;


    /**
     * State.
     */
    protected boolean parsingHeader;


    /**
     * Swallow input ? (in the case of an expectation)
     */
    protected boolean swallowInput;


    /**
     * Pointer to the current read buffer.
     */
    protected byte[] buf;


    /**
     * Pointer to the US-ASCII header buffer.
     */
    protected char[] ascbuf;


    /**
     * Last valid byte.
     */
    protected int lastValid;


    /**
     * Position in the buffer.
     */
    protected int pos;


    /**
     * HTTP header buffer no 1.
     */
    protected byte[] headerBuffer1;


    /**
     * HTTP header buffer no 2.
     */
    protected byte[] headerBuffer2;


    /**
     * HTTP body buffer.
     */
    protected byte[] bodyBuffer;


    /**
     * US-ASCII header buffer.
     */
    protected char[] headerBuffer;


    /**
     * Direct byte buffer used to perform actual reading.
     */
    protected ByteBuffer bbuf;


    /**
     * Underlying socket.
     */
    protected long socket;


    /**
     * Underlying input buffer.
     */
    protected InputBuffer inputStreamInputBuffer;


    /**
     * Filter library.
     * Note: Filter[0] is always the "chunked" filter.
     */
    protected InputFilter[] filterLibrary;


    /**
     * Active filters (in order).
     */
    protected InputFilter[] activeFilters;


    /**
     * Index of the last active filter.
     */
    protected int lastActiveFilter;


    /**
     * The socket timeout used when reading the first block of the request
     * header.
     */
    protected long readTimeout;
    
    
    // ------------------------------------------------------------- Properties


    /**
     * Set the underlying socket.
     */
    public void setSocket(long socket) {
        this.socket = socket;
        Socket.setrbb(this.socket, bbuf);
    }


    /**
     * Get the underlying socket input stream.
     */
    public long getSocket() {
        return socket;
    }


    /**
     * Add an input filter to the filter library.
     */
    public void addFilter(InputFilter filter) {

        InputFilter[] newFilterLibrary = 
            new InputFilter[filterLibrary.length + 1];
        for (int i = 0; i < filterLibrary.length; i++) {
            newFilterLibrary[i] = filterLibrary[i];
        }
        newFilterLibrary[filterLibrary.length] = filter;
        filterLibrary = newFilterLibrary;

        activeFilters = new InputFilter[filterLibrary.length];

    }


    /**
     * Get filters.
     */
    public InputFilter[] getFilters() {

        return filterLibrary;

    }


    /**
     * Clear filters.
     */
    public void clearFilters() {

        filterLibrary = new InputFilter[0];
        lastActiveFilter = -1;

    }


    /**
     * Add an input filter to the filter library.
     */
    public void addActiveFilter(InputFilter filter) {

        if (lastActiveFilter == -1) {
            filter.setBuffer(inputStreamInputBuffer);
        } else {
            for (int i = 0; i <= lastActiveFilter; i++) {
                if (activeFilters[i] == filter)
                    return;
            }
            filter.setBuffer(activeFilters[lastActiveFilter]);
        }

        activeFilters[++lastActiveFilter] = filter;

        filter.setRequest(request);

    }


    /**
     * Set the swallow input flag.
     */
    public void setSwallowInput(boolean swallowInput) {
        this.swallowInput = swallowInput;
    }


    // --------------------------------------------------------- Public Methods


    /**
     * Recycle the input buffer. This should be called when closing the 
     * connection.
     */
    public void recycle() {

        // Recycle Request object
        request.recycle();

        socket = 0;
        buf = headerBuffer1;
        lastValid = 0;
        pos = 0;
        lastActiveFilter = -1;
        parsingHeader = true;
        swallowInput = true;

    }


    /**
     * End processing of current HTTP request.
     * Note: All bytes of the current request should have been already 
     * consumed. This method only resets all the pointers so that we are ready
     * to parse the next HTTP request.
     */
    public void nextRequest()
        throws IOException {

        // Recycle Request object
        request.recycle();

        // Determine the header buffer used for next request
        byte[] newHeaderBuf = null;
        if (buf == headerBuffer1) {
            newHeaderBuf = headerBuffer2;
        } else {
            newHeaderBuf = headerBuffer1;
        }

        // Copy leftover bytes from buf to newHeaderBuf
        System.arraycopy(buf, pos, newHeaderBuf, 0, lastValid - pos);

        // Swap buffers
        buf = newHeaderBuf;

        // Recycle filters
        for (int i = 0; i <= lastActiveFilter; i++) {
            activeFilters[i].recycle();
        }

        // Reset pointers
        lastValid = lastValid - pos;
        pos = 0;
        lastActiveFilter = -1;
        parsingHeader = true;
        swallowInput = true;

    }


    /**
     * End request (consumes leftover bytes).
     * 
     * @throws IOException an undelying I/O error occured
     */
    public void endRequest()
        throws IOException {

        if (swallowInput && (lastActiveFilter != -1)) {
            int extraBytes = (int) activeFilters[lastActiveFilter].end();
            pos = pos - extraBytes;
        }

    }


    /**
     * Read the request line. This function is meant to be used during the 
     * HTTP request header parsing. Do NOT attempt to read the request body 
     * using it.
     *
     * @throws IOException If an exception occurs during the underlying socket
     * read operations, or if the given buffer is not big enough to accomodate
     * the whole line.
     * @return true if data is properly fed; false if no data is available 
     * immediately and thread should be freed
     */
    public boolean parseRequestLine(boolean useAvailableData)
        throws IOException {

        int start = 0;

        //
        // Skipping blank lines
        //

        byte chr = 0;
        do {

            // Read new bytes if needed
            if (pos >= lastValid) {
                if (useAvailableData) {
                    return false;
                }
                if (readTimeout == -1) {
                    if (!fill())
                        throw new EOFException(sm.getString("iib.eof.error"));
                } else {
                    // Do a simple read with a short timeout
                    bbuf.clear();
                    int nRead = Socket.recvbbt
                    (socket, 0, buf.length - lastValid, readTimeout);
                    if (nRead > 0) {
                        bbuf.limit(nRead);
                        bbuf.get(buf, pos, nRead);
                        lastValid = pos + nRead;
                    } else {
                        if ((-nRead) == Status.ETIMEDOUT || (-nRead) == Status.TIMEUP) {
                            return false;
                        } else {
                            throw new IOException(sm.getString("iib.failedread"));
                        }
                    }
                }
            }

            chr = buf[pos++];

        } while ((chr == Constants.CR) || (chr == Constants.LF));

        pos--;

        // Mark the current buffer position
        start = pos;

        if (pos >= lastValid) {
            if (useAvailableData) {
                return false;
            }
            if (readTimeout == -1) {
                if (!fill())
                    throw new EOFException(sm.getString("iib.eof.error"));
            } else {
                // Do a simple read with a short timeout
                bbuf.clear();
                int nRead = Socket.recvbbt
                    (socket, 0, buf.length - lastValid, readTimeout);
                if (nRead > 0) {
                    bbuf.limit(nRead);
                    bbuf.get(buf, pos, nRead);
                    lastValid = pos + nRead;
                } else {
                    if ((-nRead) == Status.ETIMEDOUT || (-nRead) == Status.TIMEUP) {
                        return false;
                    } else {
                        throw new IOException(sm.getString("iib.failedread"));
                    }
                }
            }
        }

        //
        // Reading the method name
        // Method name is always US-ASCII
        //

        boolean space = false;

        while (!space) {

            // Read new bytes if needed
            if (pos >= lastValid) {
                if (!fill())
                    throw new EOFException(sm.getString("iib.eof.error"));
            }

            ascbuf[pos] = (char) buf[pos];

            // Spec says single SP but it also says be tolerant of HT
            if (buf[pos] == Constants.SP || buf[pos] == Constants.HT) {
                space = true;
                request.method().setChars(ascbuf, start, pos - start);
            }

            pos++;

        }

        // Spec says single SP but also says be tolerant of multiple and/or HT
        while (space) {
            // Read new bytes if needed
            if (pos >= lastValid) {
                if (!fill())
                    throw new EOFException(sm.getString("iib.eof.error"));
            }
            if (buf[pos] == Constants.SP || buf[pos] == Constants.HT) {
                pos++;
            } else {
                space = false;
            }
        }
        
        // Mark the current buffer position
        start = pos;
        int end = 0;
        int questionPos = -1;

        //
        // Reading the URI
        //

        boolean eol = false;

        while (!space) {

            // Read new bytes if needed
            if (pos >= lastValid) {
                if (!fill())
                    throw new EOFException(sm.getString("iib.eof.error"));
            }

            // Spec says no CR or LF in method name
            if (buf[pos] == Constants.CR || buf[pos] == Constants.LF) {
                throw new IllegalArgumentException(
                        sm.getString("iib.invalidmethod"));
            }
            // Spec says single SP but it also says be tolerant of HT
            if (buf[pos] == Constants.SP || buf[pos] == Constants.HT) {
                space = true;
                end = pos;
            } else if ((buf[pos] == Constants.CR) 
                       || (buf[pos] == Constants.LF)) {
                // HTTP/0.9 style request
                eol = true;
                space = true;
                end = pos;
            } else if ((buf[pos] == Constants.QUESTION) 
                       && (questionPos == -1)) {
                questionPos = pos;
            }

            pos++;

        }

        // Spec says single SP but also says be tolerant of multiple and/or HT
        while (space) {
            // Read new bytes if needed
            if (pos >= lastValid) {
                if (!fill())
                    throw new EOFException(sm.getString("iib.eof.error"));
            }
            if (buf[pos] == Constants.SP || buf[pos] == Constants.HT) {
                pos++;
            } else {
                space = false;
            }
        }
        
        request.unparsedURI().setBytes(buf, start, end - start);
        if (questionPos >= 0) {
            request.queryString().setBytes(buf, questionPos + 1, 
                                           end - questionPos - 1);
            request.requestURI().setBytes(buf, start, questionPos - start);
        } else {
            request.requestURI().setBytes(buf, start, end - start);
        }

        // Mark the current buffer position
        start = pos;
        end = 0;

        //
        // Reading the protocol
        // Protocol is always US-ASCII
        //

        while (!eol) {

            // Read new bytes if needed
            if (pos >= lastValid) {
                if (!fill())
                    throw new EOFException(sm.getString("iib.eof.error"));
            }

            ascbuf[pos] = (char) buf[pos];

            if (buf[pos] == Constants.CR) {
                end = pos;
            } else if (buf[pos] == Constants.LF) {
                if (end == 0)
                    end = pos;
                eol = true;
            }

            pos++;

        }

        if ((end - start) > 0) {
            request.protocol().setChars(ascbuf, start, end - start);
        } else {
            request.protocol().setString("");
        }
        
        return true;

    }


    /**
     * Parse the HTTP headers.
     */
    public void parseHeaders()
        throws IOException {

        while (parseHeader()) {
        }

        parsingHeader = false;

    }


    /**
     * Parse an HTTP header.
     * 
     * @return false after reading a blank line (which indicates that the
     * HTTP header parsing is done
     */
    public boolean parseHeader()
        throws IOException {

        //
        // Check for blank line
        //

        byte chr = 0;
        while (true) {

            // Read new bytes if needed
            if (pos >= lastValid) {
                if (!fill())
                    throw new EOFException(sm.getString("iib.eof.error"));
            }

            chr = buf[pos];

            if ((chr == Constants.CR) || (chr == Constants.LF)) {
                if (chr == Constants.LF) {
                    pos++;
                    return false;
                }
            } else {
                break;
            }

            pos++;

        }

        // Mark the current buffer position
        int start = pos;

        //
        // Reading the header name
        // Header name is always US-ASCII
        //

        boolean colon = false;
        MessageBytes headerValue = null;

        while (!colon) {

            // Read new bytes if needed
            if (pos >= lastValid) {
                if (!fill())
                    throw new EOFException(sm.getString("iib.eof.error"));
            }

            if (buf[pos] == Constants.COLON) {
                colon = true;
                headerValue = headers.addValue(ascbuf, start, pos - start);
            }
            chr = buf[pos];
            if ((chr >= Constants.A) && (chr <= Constants.Z)) {
                buf[pos] = (byte) (chr - Constants.LC_OFFSET);
            }

            ascbuf[pos] = (char) buf[pos];

            pos++;

        }

        // Mark the current buffer position
        start = pos;
        int realPos = pos;

        //
        // Reading the header value (which can be spanned over multiple lines)
        //

        boolean eol = false;
        boolean validLine = true;

        while (validLine) {

            boolean space = true;

            // Skipping spaces
            while (space) {

                // Read new bytes if needed
                if (pos >= lastValid) {
                    if (!fill())
                        throw new EOFException(sm.getString("iib.eof.error"));
                }

                if ((buf[pos] == Constants.SP) || (buf[pos] == Constants.HT)) {
                    pos++;
                } else {
                    space = false;
                }

            }

            int lastSignificantChar = realPos;

            // Reading bytes until the end of the line
            while (!eol) {

                // Read new bytes if needed
                if (pos >= lastValid) {
                    if (!fill())
                        throw new EOFException(sm.getString("iib.eof.error"));
                }

                if (buf[pos] == Constants.CR) {
                } else if (buf[pos] == Constants.LF) {
                    eol = true;
                } else if (buf[pos] == Constants.SP) {
                    buf[realPos] = buf[pos];
                    realPos++;
                } else {
                    buf[realPos] = buf[pos];
                    realPos++;
                    lastSignificantChar = realPos;
                }

                pos++;

            }

            realPos = lastSignificantChar;

            // Checking the first character of the new line. If the character
            // is a LWS, then it's a multiline header

            // Read new bytes if needed
            if (pos >= lastValid) {
                if (!fill())
                    throw new EOFException(sm.getString("iib.eof.error"));
            }

            chr = buf[pos];
            if ((chr != Constants.SP) && (chr != Constants.HT)) {
                validLine = false;
            } else {
                eol = false;
                // Copying one extra space in the buffer (since there must
                // be at least one space inserted between the lines)
                buf[realPos] = chr;
                realPos++;
            }

        }

        // Set the header value
        headerValue.setBytes(buf, start, realPos - start);

        return true;

    }


    // ---------------------------------------------------- InputBuffer Methods


    /**
     * Read some bytes.
     */
    public int doRead(ByteChunk chunk, Request req) 
        throws IOException {

        if (lastActiveFilter == -1)
            return inputStreamInputBuffer.doRead(chunk, req);
        else
            return activeFilters[lastActiveFilter].doRead(chunk,req);

    }


    // ------------------------------------------------------ Protected Methods


    /**
     * Fill the internal buffer using data from the undelying input stream.
     * 
     * @return false if at end of stream
     */
    protected boolean fill()
        throws IOException {

        int nRead = 0;

        if (parsingHeader) {

            if (lastValid == buf.length) {
                throw new IllegalArgumentException
                    (sm.getString("iib.requestheadertoolarge.error"));
            }

            bbuf.clear();
            nRead = Socket.recvbb
                (socket, 0, buf.length - lastValid);
            if (nRead > 0) {
                bbuf.limit(nRead);
                bbuf.get(buf, pos, nRead);
                lastValid = pos + nRead;
            } else {
                if ((-nRead) == Status.EAGAIN) {
                    return false;
                } else {
                    throw new IOException(sm.getString("iib.failedread"));
                }
            }

        } else {

            buf = bodyBuffer;
            pos = 0;
            lastValid = 0;
            bbuf.clear();
            nRead = Socket.recvbb
                (socket, 0, buf.length);
            if (nRead > 0) {
                bbuf.limit(nRead);
                bbuf.get(buf, 0, nRead);
                lastValid = nRead;
            } else {
                if ((-nRead) == Status.ETIMEDOUT || (-nRead) == Status.TIMEUP) {
                    throw new SocketTimeoutException(sm.getString("iib.failedread"));
                } else {
                    throw new IOException(sm.getString("iib.failedread"));
                }
            }

        }

        return (nRead > 0);

    }


    // ------------------------------------- InputStreamInputBuffer Inner Class


    /**
     * This class is an input buffer which will read its data from an input
     * stream.
     */
    protected class SocketInputBuffer 
        implements InputBuffer {


        /**
         * Read bytes into the specified chunk.
         */
        public int doRead(ByteChunk chunk, Request req ) 
            throws IOException {

            if (pos >= lastValid) {
                if (!fill())
                    return -1;
            }

            int length = lastValid - pos;
            chunk.setBytes(buf, pos, length);
            pos = lastValid;

            return (length);

        }


    }


}
