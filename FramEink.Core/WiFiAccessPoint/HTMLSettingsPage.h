static String strHTML = "<head>"
"<title>e-radionica Inkplate e-paper display</title>"

"<style type='text/css'>"

"textarea {"
"width: 600px;"
"height: 1.5em;"
"}"

"textarea {"
"  font-size: 250%;"
"}"

"input {"
"  font-size: 400%;"
"}"

"t2 {"
"  font-size: 400%;"
"  font-weight: bold;"
"}"

"t1 {"
"  font-size: 250%;"
"  font-weight: bold;"
"}"

"</style>"

"</head>"

"<body>"
"<form action='/string/' method='get' target='_self'>"
"<t2>Settings</t2><br><br>"
"<t1>WiFi Name (SSID):</t1><br>"
"<textarea autofocus maxlength = '100' rows = '1' cols = '40' name = 'SSID'></textarea><br><br>"
"<t1>WiFi Password:</t1><br>"
"<textarea autofocus maxlength = '100' rows = '1' cols = '40' name = 'password'></textarea><br><br><br><br>"
"<t1>Weather Forecast Latitude:</t1><br>"
"<textarea autofocus maxlength = '100' rows = '1' cols = '40' name = 'latitude'></textarea><br><br>"
"<t1>Weather Forecast Longitude:</t1><br>"
"<textarea autofocus maxlength = '100' rows = '1' cols = '40' name = 'longitude'></textarea><br><br><br><br>"
"<t1>Calendar id key:</t1><br>"
"<textarea autofocus maxlength = '100' rows = '1' cols = '40' name = 'icalID'></textarea><br><br><br><br><br>"

"<input type='submit' value='Send to FramEInk!'></form></body></html>";