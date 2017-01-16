#!/usr/bin/perl

#
#  Licensed to the Apache Software Foundation (ASF) under one or more
#  contributor license agreements.  See the NOTICE file distributed with
#  this work for additional information regarding copyright ownership.
#  The ASF licenses this file to You under the Apache License, Version 2.0
#  (the "License"); you may not use this file except in compliance with
#  the License.  You may obtain a copy of the License at
# 
#      http://www.apache.org/licenses/LICENSE-2.0
# 
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#

require "subparseform.lib";
&Parse_Form;

@numbers = split(/,/, $formdata{'number'});
print "Content-type: text/html\n\n";

print "The numbers you entered were:";
foreach $number (@numbers) {
	print "<LI>$number";
	}
	
foreach $number(@numbers) {
	$number = sqrt($number);
	}

print "<P>The square roots of those numbers are: ";

foreach $number(@numbers) {
	print "<LI>$number";
	}