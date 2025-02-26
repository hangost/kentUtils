/* Copyright (C) 2014 The Regents of the University of California 
 * See README in this or parent directory for licensing information. */

#include "common.h"
#include <regex.h>
#include "hCommon.h"
#include "obscure.h"
#include "dnautil.h"
#include "errAbort.h"
#include "htmshell.h"
#include "web.h"
#include "hPrint.h"
#include "hdb.h"
#include "hui.h"
#include "hgConfig.h"
#include "cheapcgi.h"
#include "dbDb.h"
#include "hgColors.h"
#include "hubConnect.h"
#include "search.h"
#include "geoMirror.h"
#include "trackHub.h"
#include "versionInfo.h"

#ifndef GBROWSE
#include "axtInfo.h"
#include "wikiLink.h"
#include "googleAnalytics.h"
#include "jsHelper.h"
#endif /* GBROWSE */


/* flag that tell if the CGI header has already been outputed */
boolean webHeadAlreadyOutputed = FALSE;
/* flag that tell if text CGI header hsa been outputed */
boolean webInTextMode = FALSE;
static char *dbCgiName = "db";
static char *orgCgiName = "org";
static char *cladeCgiName = "clade";
static char *extraStyle = NULL;

/* globals: a cart and db for use in error handlers. */
static struct cart *errCart = NULL;
static char *errDb = NULL;

void textVaWarn(char *format, va_list args)
{
vprintf(format, args);
puts("\n");
}

void softAbort()
{
exit(0);
}

void webPushErrHandlers(void)
/* Push warn and abort handler for errAbort(). */
{
if (webInTextMode)
    pushWarnHandler(textVaWarn);
else
    pushWarnHandler(webVaWarn);
pushAbortHandler(softAbort);
hDumpStackPushAbortHandler();
}

void webPushErrHandlersCartDb(struct cart *cart, char *db)
/* Push warn and abort handler for errAbort(); save cart and db for use in handlers. */
{
errCart = cart;
errDb = db;
webPushErrHandlers();
}

void webPopErrHandlers(void)
/* Pop warn and abort handler for errAbort(). */
{
popWarnHandler();
hDumpStackPopAbortHandler();
popAbortHandler();
}

void webSetStyle(char *style)
/* set a style to add to the header */
{
extraStyle = style;
}

void webStartText()
/* output the head for a text page */
{
/*printf("Content-Type: text/plain\n\n");*/

webHeadAlreadyOutputed = TRUE;
webInTextMode = TRUE;
webPushErrHandlers();
}

static void webStartWrapperDetailedInternal(struct cart *theCart,
	char *db, char *headerText, char *textOutBuf,
	boolean withHttpHeader, boolean withLogo, boolean skipSectionHeader,
	boolean withHtmlHeader)
/* output a CGI and HTML header with the given title in printf format */
{
char uiState[256];
char *scriptName = cgiScriptName();
boolean isEncode = FALSE;
if (theCart)
    {
    char *theGenome = NULL;
    char *genomeEnc = NULL;

    getDbAndGenome(theCart, &db, &theGenome, NULL);
    genomeEnc = cgiEncode(theGenome);

    safef(uiState, sizeof(uiState), "?%s=%s&%s=%s&%s=%s",
	     orgCgiName, genomeEnc,
	     dbCgiName, db,
	     cartSessionVarName(), cartSessionId(theCart));
    }
else
    {
    uiState[0] = 0;
    uiState[1] = 0;
    }
if (db == NULL)
    db = hDefaultDb();
boolean dbIsFound = hDbExists(db);
boolean haveBlat = FALSE;
if (dbIsFound)
    haveBlat = hIsBlatIndexedDatabase(db);

if (scriptName == NULL)
    scriptName = cloneString("");
/* don't output two headers */
if(webHeadAlreadyOutputed)
    return;

if (sameString(cgiUsualString("action",""),"encodeReleaseLog") ||
    rStringIn("EncodeDataVersions", scriptName))
        isEncode = TRUE;

/* Preamble. */
dnaUtilOpen();

if (withHttpHeader)
    puts("Content-type:text/html\n");

if (withHtmlHeader)
    {
    char *newString, *ptr1, *ptr2;

    char *browserVersion;
    if (btIE == cgiClientBrowser(&browserVersion, NULL, NULL) && *browserVersion < '8')
        puts("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">");
    else
        puts("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" "
             "\"http://www.w3.org/TR/html4/loose.dtd\">");
    // Strict would be nice since it fixes atleast one IE problem (use of :hover CSS pseudoclass)
    puts(
	"<HTML>" "\n"
	"<HEAD>" "\n"
	);
    printf("\t%s\n", headerText);
    printf("\t<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html;CHARSET=iso-8859-1\">" "\n"
	 "\t<META http-equiv=\"Content-Script-Type\" content=\"text/javascript\">" "\n"
         "\t<META HTTP-EQUIV=\"Pragma\" CONTENT=\"no-cache\">" "\n"
         "\t<META HTTP-EQUIV=\"Expires\" CONTENT=\"-1\">" "\n"
	 "\t<TITLE>"
	 );
    /* we need to take any HTML formatting out of the titlebar string */
    newString = cloneString(textOutBuf);

    for(ptr1=newString, ptr2=textOutBuf; *ptr2 ; ptr2++)
	{
	if (*ptr2 == '<')
	    {
	    for(; *ptr2 && (*ptr2 != '>'); ptr2++)
		;
	    }
	else
	    *ptr1++ = *ptr2;
	}
    *ptr1 = 0;
    htmlTextOut(newString);
    printf("	</TITLE>\n    ");
    webIncludeResourceFile("HGStyle.css");
    if (extraStyle != NULL)
        puts(extraStyle);
    printf("</HEAD>\n");
    printBodyTag(stdout);
    htmlWarnBoxSetup(stdout);// Sets up a warning box which can be filled with errors as they occur
    puts(commonCssStyles());
    }
puts(
    "<A NAME=\"TOP\"></A>" "\n"
    "" "\n"
    "<TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0 WIDTH=\"100%\">" "\n");

if (withLogo)
    {
    puts("<TR><TH COLSPAN=1 ALIGN=\"left\">");
    if (isEncode)
	{
	puts("<A HREF=\"http://www.genome.gov/10005107\" TARGET=\"_BLANK\">"
	     "<IMG SRC=\"../images/ENCODE_scaleup_logo.png\" height=50 ALT=\"ENCODE Project at NHGRI\">"
	     "</A>");
	puts("<IMG SRC=\"../images/encodeDcc.jpg\" ALT=\"ENCODE Project at UCSC\">");
	}
    else
	{
	puts("<IMG SRC=\"../images/title.jpg\">");
	}
    puts("</TH></TR>" "\n"
         "" "\n" );
    }

/* Put up the hot links bar. */

char *menuStr = menuBar(theCart);
if(menuStr)
    {
    puts(menuStr);
    }

if (endsWith(scriptName, "hgGateway") && geoMirrorEnabled())
    {
    // Show an opt-out alert if user is on a host to which user has been automatically redirected (just once, right after they have been redirected)
    char *source = cgiOptionalString("source");
    char *redirect = cgiOptionalString("redirect");
    if (source != NULL && redirect != NULL && sameString(redirect, "auto"))
	{
	char *domain = cgiServerName();
	char *port = cgiServerPort();
        // We don't bother maintaining stuff in request URI, because it may contain items like hgsid and other host specific values
        int newUriSize = 2048;
	char *newUri = needMem(newUriSize);
	safef(newUri, newUriSize, "http%s://%s:%s/cgi-bin/hgGateway?redirect=manual&source=%s", 
	    cgiServerHttpsIsOn() ? "s" : "", source, port, domain);

	printf("<TR><TD COLSPAN=3 id='redirectTd' onclick=\"javascript:document.getElementById('redirectTd').innerHTML='';\">"
	    "<div style=\"margin: 10px 25%%; border-style:solid; border-width:thin; border-color:#97D897;\">"
	    "<h3 style=\"background-color: #97D897; text-align: left; margin-top:0px; margin-bottom:0px;\">"
	    "&nbsp;You've been redirected to your nearest mirror - %s"
	    "<idiv style=\"float:right;\">[x]</idiv>"
	    "</h3> "
	    "<ul style=\"margin:5px;\">"
	    "<li>Take me back to <a href=\"%s\">%s</a>"
	    "<idiv style=\"float:right;\"><a href=\"../goldenPath/help/genomeEuro.html\">What is this?</a></idiv>"
	    "</li>"
	    "</ul>"
	    "</div>"
	    "</TD></TR>\n"
	    , domain, newUri, source );
	}
    }

if(!skipSectionHeader)
/* this HTML must be in calling code if skipSectionHeader is TRUE */
    {
    puts( // TODO: Replace nested tables with CSS (difficulty is that tables are closed elsewhere)
         "<!-- +++++++++++++++++++++ CONTENT TABLES +++++++++++++++++++ -->" "\n"
         "<TR><TD COLSPAN=3>\n"
         "<div id=firstSection>"
         "      <!--outer table is for border purposes-->\n"
         "      <TABLE WIDTH='100%' BGCOLOR='#" HG_COL_BORDER "' BORDER='0' CELLSPACING='0' "
                     "CELLPADDING='1'><TR><TD>\n"
         "    <TABLE BGCOLOR='#" HG_COL_INSIDE "' WIDTH='100%'  BORDER='0' CELLSPACING='0' "
                     "CELLPADDING='0'><TR><TD>\n"
         "     <div class='subheadingBar'><div class='windowSize' id='sectTtl'>"
         );
    htmlTextOut(textOutBuf);

    puts("     </div></div>\n"
         "     <TABLE BGCOLOR='#" HG_COL_INSIDE "' WIDTH='100%' CELLPADDING=0>"
              "<TR><TH HEIGHT=10></TH></TR>\n"
         "     <TR><TD WIDTH=10>&nbsp;</TD><TD>\n\n"
         );
    };
webPushErrHandlers();
/* set the flag */
webHeadAlreadyOutputed = TRUE;
}	/*	static void webStartWrapperDetailedInternal()	*/

void webStartWrapperDetailedArgs(struct cart *theCart, char *db,
	char *headerText, char *format, va_list args, boolean withHttpHeader,
	boolean withLogo, boolean skipSectionHeader, boolean withHtmlHeader)
/* output a CGI and HTML header with the given title in printf format */
{
char textOutBuf[1024];
va_list argscp;

va_copy(argscp,args);
vasafef(textOutBuf, sizeof(textOutBuf), format, argscp);
va_end(argscp);

webStartWrapperDetailedInternal(theCart, db, headerText, textOutBuf,
	withHttpHeader, withLogo, skipSectionHeader, withHtmlHeader);
}

void webStartWrapperDetailedNoArgs(struct cart *theCart, char *db,
	char *headerText, char *format, boolean withHttpHeader,
	boolean withLogo, boolean skipSectionHeader, boolean withHtmlHeader)
/* output a CGI and HTML header with the given title in printf format */
{
char textOutBuf[512];

safecpy(textOutBuf, sizeof(textOutBuf), format);
webStartWrapperDetailedInternal(theCart, db, headerText, textOutBuf,
	withHttpHeader, withLogo, skipSectionHeader, withHtmlHeader);
}

void webStartWrapperGatewayHeader(struct cart *theCart, char *db,
	char *headerText, char *format, va_list args, boolean withHttpHeader,
	boolean withLogo, boolean skipSectionHeader)
{
webStartWrapperDetailedArgs(theCart, db, headerText, format, args, withHttpHeader,
	withLogo, skipSectionHeader, TRUE);
}

void webStartWrapperGateway(struct cart *theCart, char *db, char *format, va_list args, boolean withHttpHeader, boolean withLogo, boolean skipSectionHeader)
/* output a CGI and HTML header with the given title in printf format */
{
webStartWrapperGatewayHeader(theCart, db, "", format, args, withHttpHeader,
			     withLogo, skipSectionHeader);
}

void webStartWrapper(struct cart *theCart, char *db, char *format, va_list args, boolean withHttpHeader, boolean withLogo)
    /* allows backward compatibility with old webStartWrapper that doesn't contain the "skipHeader" arg */
	/* output a CGI and HTML header with the given title in printf format */
{
webStartWrapperGatewayHeader(theCart, db, "", format, args, withHttpHeader,
                             withLogo, FALSE);
}

void webStart(struct cart *theCart, char *db, char *format, ...)
/* Print out pretty wrapper around things when not
 * from cart. */
{
va_list args;
va_start(args, format);
webStartWrapper(theCart, db, format, args, TRUE, TRUE);
va_end(args);
}

void webStartHeader(struct cart *theCart, char *db, char *headerText, char *format, ...)
/* Print out pretty wrapper around things when not from cart.
 * Include headerText in the html header. */
{
va_list args;
va_start(args, format);
webStartWrapperGatewayHeader(theCart, db, headerText, format, args, TRUE, TRUE,
			     FALSE);
va_end(args);
}


void webEndSection()
/* Close down a section */
{
puts(
    "" "\n"
    "	</TD><TD WIDTH=15></TD></TR></TABLE>" "\n"
//    "<BR>"
    "	</TD></TR></TABLE>" "\n"
    "	</TD></TR></TABLE>" "\n"
    "	" );
puts("</div>");
}

void webNewSectionHeaderStart(boolean hasTitle)
/* Start the header for a new section on the web page.
 * May be used to maintain table layout without a proper section header */
{
webEndSection();
puts("<div>");
puts("<!-- +++++++++++++++++++++ START NEW SECTION +++++++++++++++++++ -->");
puts(  // TODO: Replace nested tables with CSS (difficulty is that tables are closed elsewhere)
    "<BR>\n\n"
    "   <!--outer table is for border purposes-->\n"
    "   <TABLE WIDTH='100%' BGCOLOR='#" HG_COL_BORDER
        "' BORDER='0' CELLSPACING='0' CELLPADDING='1'><TR><TD>\n"
    "    <TABLE BGCOLOR='#" HG_COL_INSIDE
         "' WIDTH='100%'  BORDER='0' CELLSPACING='0' CELLPADDING='0'><TR><TD>\n");
if (hasTitle)
    puts("<div class='subheadingBar' class='windowSize'>");
else
    puts("<div>");
}

void webNewSectionHeaderEnd()
/* Properly close header of collapsible section on web page */
{
puts("     </div>\n"
     "     <TABLE BGCOLOR='#" HG_COL_INSIDE "' WIDTH='100%' CELLPADDING=0>"
          "<TR><TH HEIGHT=10></TH></TR>\n"
     "     <TR><TD WIDTH=10>&nbsp;</TD><TD>\n\n");
}

void webNewSection(char* format, ...)
/* create a new section on the web page */
{
va_list args;
va_start(args, format);
webNewSectionHeaderStart(TRUE);
vprintf(format, args);
webNewSectionHeaderEnd();
va_end(args);
}

void webNewEmptySection()
/* create a new section on the web page to maintain table layout */
{
webNewSectionHeaderStart(FALSE);
webNewSectionHeaderEnd();
}

void webEndSectionTables()
/* Finish with section tables (but don't do /BODY /HTML like
 * webEnd does. */
{
webEndSection();
puts("</TD></TR></TABLE>\n");
}

void webEnd()
/* output the footer of the HTML page */
{
if(!webInTextMode)
    {
    webEndSectionTables();
#ifndef GBROWSE
    googleAnalytics();
#endif /* GBROWSE */
    puts( "</BODY></HTML>");
    webPopErrHandlers();
    }
}

static boolean gotWarnings = FALSE;

void webVaWarn(char *format, va_list args)
/* Warning handler that closes out page and stuff in
 * the fancy form. */
{
gotWarnings = TRUE;
boolean needStart = !webHeadAlreadyOutputed;
if (needStart)
    {
    // All callers of this (via webPushErrHandlersCartDb) have skipped Content-type
    // because they want to output text unless we hit this condition:
    puts("Content-type:text/html\n");
    cartWebStart(errCart, errDb, "Error");
    }
htmlVaWarn(format, args);
printf("\n<!-- HGERROR -->\n\n\n");
}


boolean webGotWarnings()
/* Return TRUE if webVaWarn has been called. */
{
return gotWarnings;
}

void webAbort(char* title, char* format, ...)
/* an abort function that outputs a error page */
{
va_list args;
va_start(args, format);

/* output the header */
if(!webHeadAlreadyOutputed)
    webStart(errCart, NULL, title);

/* in text mode, have a different error */
if(webInTextMode)
	printf("\n\n\n          %s\n\n", title);

vprintf(format, args);
printf("<!-- HGERROR -->\n");
printf("\n\n");

webEnd();

va_end(args);
exit(0);
}

void printCladeListHtml(char *genome, char *onChangeText)
/* Make an HTML select input listing the clades. */
{
char **row = NULL;
char *clades[128];
char *labels[128];
char *defaultClade = hClade(genome);
char *defaultLabel = NULL;
int numClades = 0;

struct sqlConnection *conn = hConnectCentral();  // after hClade since it access hgcentral too
// get only the clades that have actual active genomes
struct sqlResult *sr = sqlGetResult(conn, "NOSQLINJ SELECT DISTINCT(c.name), c.label FROM clade c, genomeClade g, dbDb d WHERE c.name=g.clade AND d.organism=g.genome AND d.active=1 ORDER BY c.priority");
while ((row = sqlNextRow(sr)) != NULL)
    {
    clades[numClades] = cloneString(row[0]);
    labels[numClades] = cloneString(row[1]);
    if (sameWord(defaultClade, clades[numClades]))
	defaultLabel = clades[numClades];
    numClades++;
    if (numClades >= ArraySize(clades))
        internalErr();
    }
sqlFreeResult(&sr);
hDisconnectCentral(&conn);

struct slPair *names = trackHubGetCladeLabels();

for(; names; names = names->next)
    {
    clades[numClades] = names->name;
    labels[numClades] = names->val;
    if (sameWord(defaultClade, clades[numClades]))
	defaultLabel = clades[numClades];
    numClades++;
    if (numClades >= ArraySize(clades))
        internalErr();
    }

cgiMakeDropListFull(cladeCgiName, labels, clades, numClades,
                    defaultLabel, onChangeText);
}

static void printSomeGenomeListHtmlNamedMaybeCheck(char *customOrgCgiName,
	 char *db, struct dbDb *dbList, char *onChangeText, boolean doCheck)
/* Prints to stdout the HTML to render a dropdown list
 * containing a list of the possible genomes to choose from.
 * param db - a database whose genome will be the default genome.
 *                       If NULL, no default selection.
 * param onChangeText - Optional (can be NULL) text to pass in
 *                              any onChange javascript. */
{
char *orgList[1024];
int numGenomes = 0;
struct dbDb *cur = NULL;
struct hash *hash = hashNew(10); // 2^^10 entries = 1024
char *selGenome = hGenomeOrArchive(db);
char *values [1024];
char *cgiName;

for (cur = dbList; cur != NULL; cur = cur->next)
    {
    if (!hashFindVal(hash, cur->genome) &&
	(!doCheck || hDbExists(cur->name)))
        {
        hashAdd(hash, cur->genome, cur);
        orgList[numGenomes] = trackHubSkipHubName(cur->genome);
        values[numGenomes] = cur->genome;
        numGenomes++;
	if (numGenomes >= ArraySize(orgList))
	    internalErr();
        }
    }

cgiName = (customOrgCgiName != NULL) ? customOrgCgiName : orgCgiName;
cgiMakeDropListFull(cgiName, orgList, values, numGenomes,
                    selGenome, onChangeText);
hashFree(&hash);
}

void printSomeGenomeListHtmlNamed(char *customOrgCgiName, char *db, struct dbDb *dbList, char *onChangeText)
/* Prints to stdout the HTML to render a dropdown list
 * containing a list of the possible genomes to choose from.
 * param db - a database whose genome will be the default genome.
 *                       If NULL, no default selection.
 * param onChangeText - Optional (can be NULL) text to pass in
 *                              any onChange javascript. */
{
return printSomeGenomeListHtmlNamedMaybeCheck(customOrgCgiName, db, dbList,
					      onChangeText, TRUE);
}

void printLiftOverGenomeList(char *customOrgCgiName, char *db,
			     struct dbDb *dbList, char *onChangeText)
/* Prints to stdout the HTML to render a dropdown list
 * containing a list of the possible genomes to choose from.
 * Databases in dbList do not have to exist.
 * param db - a database whose genome will be the default genome.
 *                       If NULL, no default selection.
 * param onChangeText - Optional (can be NULL) text to pass in
 *                              any onChange javascript. */
{
return printSomeGenomeListHtmlNamedMaybeCheck(customOrgCgiName, db, dbList,
					      onChangeText, FALSE);
}

void printSomeGenomeListHtml(char *db, struct dbDb *dbList, char *onChangeText)
/* Prints the dropdown list using the orgCgiName */
{
printSomeGenomeListHtmlNamed(NULL, db, dbList, onChangeText);
}

void printGenomeListHtml(char *db, char *onChangeText)
/* Prints to stdout the HTML to render a dropdown list
 * containing a list of the possible genomes to choose from.
 * param db - a database whose genome will be the default genome.
 *                       If NULL, no default selection.
 * param onChangeText - Optional (can be NULL) text to pass in
 *                              any onChange javascript. */
{
printSomeGenomeListHtml(db, hGetIndexedDatabases(), onChangeText);
}

void printBlatGenomeListHtml(char *db, char *onChangeText)
/* Prints to stdout the HTML to render a dropdown list
 * containing a list of the possible genomes to choose from.
 * param db - a database whose genome will be the default genome.
 *                       If NULL, no default selection.
 * param onChangeText - Optional (can be NULL) text to pass in
 *                              any onChange javascript. */
{
printSomeGenomeListHtml(db, hGetBlatIndexedDatabases(), onChangeText);
}


void printGenomeListForCladeHtml(char *db, char *onChangeText)
/* Prints to stdout the HTML to render a dropdown list containing
 * a list of the possible genomes from selOrganism's clade to choose from.
 * selOrganism is the default for the select.
 */
{
printSomeGenomeListHtml(db, hGetIndexedDatabasesForClade(db), onChangeText);
}

void printAllAssemblyListHtmlParm(char *db, struct dbDb *dbList,
                            char *dbCgi, bool allowInactive, char *javascript)
/* Prints to stdout the HTML to render a dropdown list containing the list
 * of assemblies for the current genome to choose from.  By default,
 * this includes only active assemblies with a database (with the
 * exception of the default assembly, which will be included even
 * if it isn't active).
 *  param db - The default assembly (the database name) to choose as selected.
 *             If NULL, no default selection.
 *  param allowInactive - if set, print all assemblies for this genome,
 *                        even if they're inactive or have no database
 */
{

char *assemblyList[128];
char *values[128];
int numAssemblies = 0;
struct dbDb *cur = NULL;
char *genome = hGenomeOrArchive(db);
char *selAssembly = NULL;

if (genome == NULL)
#ifdef LOWELAB
    genome = "Pyrococcus furiosus";
#else
    genome = "Human";
#endif
for (cur = dbList; cur != NULL; cur = cur->next)
    {
    /* Only for this genome */
    if (!sameWord(genome, cur->genome))
        continue;

    /* Save a pointer to the current assembly */
    if (sameWord(db, cur->name))
        selAssembly = cur->name;

    if (allowInactive ||
        ((cur->active || sameWord(cur->name, db))
                && (trackHubDatabase(db) || sqlDatabaseExists(cur->name))))
        {
        assemblyList[numAssemblies] = cur->description;
        values[numAssemblies] = cur->name;
        numAssemblies++;
	if (numAssemblies >= ArraySize(assemblyList))
	    internalErr();
        }

    }
cgiMakeDropListFull(dbCgi, assemblyList, values, numAssemblies,
                                selAssembly, javascript);
}

void printSomeAssemblyListHtmlParm(char *db, struct dbDb *dbList,
                                        char *dbCgi, char *javascript)
/* Find all the assemblies from the list that are active.
 * Prints to stdout the HTML to render a dropdown list containing the list
 * of the possible assemblies to choose from.
 * param db - The default assembly (the database name) to choose as selected.
 *    If NULL, no default selection.  */
{

    printAllAssemblyListHtmlParm(db, dbList, dbCgi, TRUE, javascript);
}

void printSomeAssemblyListHtml(char *db, struct dbDb *dbList, char *javascript)
/* Find all assemblies from the list that are active, and print
 * HTML to render dropdown list
 * param db - default assembly.  If NULL, no default selection */
{
printSomeAssemblyListHtmlParm(db, dbList, dbCgiName, javascript);
}

void printAssemblyListHtml(char *db, char *javascript)
/* Find all the assemblies that pertain to the selected genome
 * Prints to stdout the HTML to render a dropdown list containing
 * a list of the possible assemblies to choose from.
 * Param db - The assembly (the database name) to choose as selected.
 * If NULL, no default selection.  */
{
struct dbDb *dbList = hGetIndexedDatabases();
printSomeAssemblyListHtml(db, dbList, javascript);
}

void printAssemblyListHtmlExtra(char *db, char *javascript)
{
/* Find all the assemblies that pertain to the selected genome
Prints to stdout the HTML to render a dropdown list containing a list of the possible
assemblies to choose from.

param curDb - The assembly (the database name) to choose as selected.
If NULL, no default selection.
 */
struct dbDb *dbList = hGetIndexedDatabases();
printSomeAssemblyListHtmlParm(db, dbList, dbCgiName, javascript);
}

void printBlatAssemblyListHtml(char *db)
{
/* Find all the assemblies that pertain to the selected genome
Prints to stdout the HTML to render a dropdown list containing a list of the possible
assemblies to choose from.

param curDb - The assembly (the database name) to choose as selected.
If NULL, no default selection.
 */
struct dbDb *dbList = hGetBlatIndexedDatabases();
printSomeAssemblyListHtml(db, dbList, NULL);
}

void printOrgAssemblyListAxtInfo(char *dbCgi, char *javascript)
/* Find all the organisms/assemblies that are referenced in axtInfo,
 * and print the dropdown list. */
{
struct dbDb *dbList = hGetAxtInfoDbs(dbCgi);
char *assemblyList[128];
char *values[128];
int numAssemblies = 0;
struct dbDb *cur = NULL;
char *assembly = cgiOptionalString(dbCgi);
char orgAssembly[256];

for (cur = dbList; ((cur != NULL) && (numAssemblies < 128)); cur = cur->next)
    {
    safef(orgAssembly, sizeof(orgAssembly), "%s %s",
	  cur->organism, cur->description);
    assemblyList[numAssemblies] = cloneString(orgAssembly);
    values[numAssemblies] = cur->name;
    numAssemblies++;
    }

#ifdef OLD
// Have to use the "menu" name, not the value, to mark selected:
if (assembly != NULL)
    {
    char *selOrg    = hOrganism(assembly);
    char *selFreeze = hFreezeFromDb(assembly);
    safef(orgAssembly, sizeof(orgAssembly), "%s %s", selOrg, selFreeze);
    assembly = cloneString(orgAssembly);
    }
#endif /* OLD */

cgiMakeDropListFull(dbCgi, assemblyList, values, numAssemblies, assembly,
		    javascript);
}

static char *getDbForGenome(char *genome, struct cart *cart)
/*
  Function to find the default database for the given Genome.
It looks in the cart first and then, if that database's Genome matches the
passed-in Genome, returns it. If the Genome does not match, it returns the default
database that does match that Genome.

param Genome - The Genome for which to find a database
param cart - The cart to use to first search for a suitable database name
return - The database matching this Genome type
*/
{

char *retDb = cartUsualString(cart, dbCgiName, NULL);

if ((retDb == NULL) || !hDbExists(retDb))
    {
    retDb = hDefaultDb();
    }

/* If genomes don't match, then get the default db for that genome */
if (differentWord(genome, hGenome(retDb)))
    {
    retDb = hDefaultDbForGenome(genome);
    }

return retDb;
}

void getDbGenomeClade(struct cart *cart, char **retDb, char **retGenome,
		      char **retClade, struct hash *oldVars)
/* Examine CGI and cart variables to determine which db, genome, or clade
 *  has been selected, and then adjust as necessary so that all three are
 * consistent.  Detect changes and reset db-specific cart variables.
 * Save db, genome and clade in the cart so it will be consistent hereafter.
 * The order of preference here is as follows:
 * If we got a request that explicitly names the db, that takes
 * highest priority, and we synch the organism to that db.
 * If we get a cgi request for a specific organism then we use that
 * organism to choose the DB.  If just clade, go from there.

 * In the cart only, we use the same order of preference.
 * If someone requests an Genome we try to give them the same db as
 * was in their cart, unless the Genome doesn't match.
 */
{
boolean gotClade = hGotClade();
*retDb = cgiOptionalString(dbCgiName);
*retGenome = cgiOptionalString(orgCgiName);
*retClade = cgiOptionalString(cladeCgiName);

/* Was the database passed in as a cgi param?
 * If so, it takes precedence and determines the genome. */
if (*retDb && hDbExists(*retDb))
    {
    *retGenome = hGenome(*retDb);
    }
/* If no db was passed in as a cgi param then was the organism (a.k.a. genome)
 * passed in as a cgi param?
 * If so, the we use the proper database for that genome. */
else if (*retGenome && !sameWord(*retGenome, "0"))
    {
    *retDb = getDbForGenome(*retGenome, cart);
    *retGenome = hGenome(*retDb);
    }
else if (*retClade && gotClade)
    {
    *retGenome = hDefaultGenomeForClade(*retClade);
    *retDb = getDbForGenome(*retGenome, cart);
    }
/* If no cgi params passed in then we need to inspect the session */
else
    {
    *retDb = cartOptionalString(cart, dbCgiName);
    *retGenome = cartOptionalString(cart, orgCgiName);
    *retClade = cartOptionalString(cart, cladeCgiName);
    /* If there was a db found in the session that determines everything. */
    if (*retDb && hDbExists(*retDb))
        {
        *retGenome = hGenome(*retDb);
        }
    else if (*retGenome && !sameWord(*retGenome, "0"))
	{
	*retDb = hDefaultDbForGenome(*retGenome);
	}
    else if (*retClade && gotClade)
	{
        *retGenome = hDefaultGenomeForClade(*retClade);
	*retDb = getDbForGenome(*retGenome, cart);
	}
    /* If no organism in the session then get the default db and organism. */
    else
	{
	*retDb = hDefaultDb();
	*retGenome = hGenome(*retDb);
        }
    }
*retDb = cloneString(*retDb);
*retGenome = cloneString(*retGenome);
*retClade = hClade(*retGenome);

if ( (*retClade != NULL) && sameString(*retClade, "none"))
    cartRemove(cart, "position");
/* Detect change of database and reset db-specific cart variables: */
if (oldVars)
    {
    char *oldDb = hashFindVal(oldVars, "db");
    char *oldOrg = hashFindVal(oldVars, "org");
    char *oldClade = hashFindVal(oldVars, "clade");
    if ((!IS_CART_VAR_EMPTY(oldDb)    && differentWord(oldDb, *retDb)) ||
        (!IS_CART_VAR_EMPTY(oldOrg)   && differentWord(oldOrg, *retGenome)) ||
        (!IS_CART_VAR_EMPTY(oldClade) && differentWord(oldClade, *retClade)))
	{
	/* Change position to default -- unless it was passed in via CGI: */
	if (cgiOptionalString("position") == NULL)
	    cartSetString(cart, "position", hDefaultPos(*retDb));
	/* hgNear search term -- unless it was passed in via CGI: */
	if (cgiOptionalString("near_search") == NULL)
	    cartRemove(cart, "near_search");
	/* hgBlat results (hgUserPsl track): */
	cartRemove(cart, "ss");
	/* hgTables correlate: */
	cartRemove(cart, "hgta_correlateTrack");
	cartRemove(cart, "hgta_correlateTable");
	cartRemove(cart, "hgta_correlateGroup");
	cartRemove(cart, "hgta_correlateOp");
	cartRemove(cart, "hgta_nextCorrelateTrack");
	cartRemove(cart, "hgta_nextCorrelateTable");
	cartRemove(cart, "hgta_nextCorrelateGroup");
	cartRemove(cart, "hgta_nextCorrelateOp");
	cartRemove(cart, "hgta_corrWinSize");
	cartRemove(cart, "hgta_corrMaxLimitCount");
	}
    }

/* Save db, genome (as org) and clade in cart. */
cartSetString(cart, "db", *retDb);
cartSetString(cart, "org", *retGenome);
if (gotClade)
    cartSetString(cart, "clade", *retClade);
}

void getDbAndGenome(struct cart *cart, char **retDb, char **retGenome,
		    struct hash *oldVars)
/* Get just the db and genome. */
{
char *garbage = NULL;
getDbGenomeClade(cart, retDb, retGenome, &garbage, oldVars);
freeMem(garbage);
}

void webIncludeFile(char *file)
/* Include an HTML file in a CGI.
 *   The file path may begin with hDocumentRoot(); if it doesn't, it is
 *   assumed to be relative and hDocumentRoot() will be prepended. */
{
char *str = hFileContentsOrWarning(file);
puts(str);
freeMem(str);
}

void webIncludeHelpFile(char *fileRoot, boolean addHorizLine)
/* Given a help file root name (e.g. "hgPcrResult" or "cutters"),
 * print out the contents of the file.  If addHorizLine, print out an
 * <HR> first. */
{
if (addHorizLine)
    htmlHorizontalLine();
webIncludeFile(hHelpFile(fileRoot));
}

void webPrintLinkTableStart()
/* Print link table start in our colors. */
{
printf("<TABLE><TR><TD BGCOLOR='#888888'>\n");
printf("<TABLE CELLSPACING=1 CELLPADDING=3><TR>\n");
}

void webPrintLinkTableEnd()
/* Print link table end in our colors. */
{
printf("</TR></TABLE>\n");
printf("</TD></TR></TABLE>\n");
}

void webPrintLinkOutCellStart()
/* Print link cell that goes out of our site. End with
 * webPrintLinkTableEnd. */
{
printf("<TD BGCOLOR='#" HG_COL_LOCAL_TABLE "'>");
}

void webPrintWideCellStart(int colSpan, char *bgColorRgb)
/* Print link multi-column cell start in our colors. */
{
printf("<TD BGCOLOR='#%s'", bgColorRgb);
if (colSpan > 1)
    printf(" COLSPAN=%d", colSpan);
printf(">");
}

void webPrintLinkCellStart()
/* Print link cell start in our colors. */
{
webPrintWideCellStart(1, HG_COL_TABLE);
}

void webPrintLinkCellRightStart()
/* Print right-justified cell start in our colors. */
{
printf("<TD BGCOLOR='#"HG_COL_TABLE"' ALIGN='right'>");
}

void webPrintLinkCellEnd()
/* Print link cell end in our colors. */
{
printf("</TD>");
}

void webPrintLinkCell(char *link)
/* Print link cell in our colors, if links is null, print empty cell */
{
webPrintLinkCellStart();
if (link != NULL)
    puts(link);
webPrintLinkCellEnd();
}

void webPrintIntCell(int val)
/* Print right-justified int cell in our colors. */
{
webPrintLinkCellRightStart();
printf("%d", val);
webPrintLinkCellEnd();
}

void webPrintDoubleCell(double val)
/* Print right-justified cell in our colors with two digits to right of decimal. */
{
webPrintLinkCellRightStart();
printf("%4.2f", val);
webPrintLinkCellEnd();
}

void webPrintWideLabelCell(char *label, int colSpan)
/* Print label cell over multiple columns in our colors. */
{
printf("<TD BGCOLOR='#"HG_COL_TABLE_LABEL"'");
if (colSpan > 1)
    printf(" COLSPAN=%d", colSpan);
printf("><span style='color:#FFFFFF;'><B>%s</B></span></TD>", label);
}

void webPrintWideCenteredLabelCell(char *label, int colSpan)
/* Print label cell over multiple columns in our colors and centered. */
{
printf("<TD BGCOLOR='#" HG_COL_TABLE_LABEL "'");
if (colSpan > 1)
    printf(" COLSPAN=%d", colSpan);
printf("><CENTER><span style='color:#FFFFFF;'><B>%s</B></span></CENTER></TD>", label);
}

void webPrintLabelCell(char *label)
/* Print label cell in our colors. */
{
webPrintWideLabelCell(label, 1);
}

void webPrintLinkTableNewRow()
/* start a new row */
{
printf("</TR>\n<TR>");
}

void finishPartialTable(int rowIx, int itemPos, int maxPerRow,
	void (*cellStart)())
/* Fill out partially empty last row. */
{
if (rowIx != 0 && itemPos < maxPerRow)
    {
    int i;
    for (i=itemPos; i<maxPerRow; ++i)
        {
	cellStart();
	webPrintLinkCellEnd();
	}
    }
}

void webFinishPartialLinkOutTable(int rowIx, int itemPos, int maxPerRow)
/* Fill out partially empty last row. */
{
finishPartialTable(rowIx, itemPos, maxPerRow, webPrintLinkOutCellStart);
}

void webFinishPartialLinkTable(int rowIx, int itemPos, int maxPerRow)
/* Fill out partially empty last row. */
{
finishPartialTable(rowIx, itemPos, maxPerRow, webPrintLinkCellStart);
}

char *webTimeStampedLinkToResource(char *fileName, boolean wrapInHtml)
// If wrapInHtml
//   returns versioned link embedded in style or script html (free after use).
// else
//   returns full path of a versioned path to the requested resource file (js, or css).
// NOTE: png, jpg and gif should also be supported but are untested.
//
// In production sites we use a versioned softlink that includes the CGI version. This has the following benefits:
// a) flushes user's web browser cache when the user visits a GB site whose version has changed since their last visit;
// b) enforces the requirement that static files are the same version as the CGIs (something that often fails to happen in mirrors).
// (see notes in redmine #3170).
//
// In dev trees we use mtime to create a pseudo-version; this forces web browsers to reload css/js file when it changes,
// so we don't get odd behavior that can be caused by caching of mis-matched javascript and style files in dev trees.
//
// In either case, the actual file has to have been previously created by running make in the appropriate directory (kent/src/hg/js
// or kent/src/hg/htdocs/style).
{
char baseName[PATH_LEN];
char extension[FILEEXT_LEN];
splitPath(fileName, NULL, baseName, extension);
boolean js = sameString(".js",extension);
boolean style = !js && sameString(".css",extension);
boolean image = !js
             && !style
             && (  sameString(".png",extension)
                || sameString(".jpg",extension)
                || sameString(".gif",extension));
if (!js && !style) // && !image) NOTE: This code has not been tested on images but should work.
    errAbort("webTimeStampedLinkToResource: unknown resource type for %s.\n", fileName);

// Build and verify directory
char *dirName = "";
if (js)
    dirName = cfgOptionDefault("browser.javaScriptDir", "js");
else if (style)
    dirName = cfgOptionDefault("browser.styleDir","style");
else if (image)
    dirName = cfgOptionDefault("browser.styleImagesDir","style/images");
struct dyString *fullDirName = NULL;
char *docRoot = hDocumentRoot();
if (docRoot != NULL)
    fullDirName = dyStringCreate("%s/%s", docRoot, dirName);
else
    // tolerate missing docRoot (i.e. when running from command line)
    fullDirName = dyStringCreate("%s", dirName);
if (!fileExists(dyStringContents(fullDirName)))
    errAbort("webTimeStampedLinkToResource: dir: %s doesn't exist.\n",
             dyStringContents(fullDirName));

// build and verify real path to file
struct dyString *realFileName = dyStringCreate("%s/%s", dyStringContents(fullDirName), fileName);
if (!fileExists(dyStringContents(realFileName)))
    errAbort("webTimeStampedLinkToResource: file: %s doesn't exist.\n",
             dyStringContents(realFileName));

// build and verify link path including timestamp in the form of dir/baseName + timeStamp or CGI Version + ext
long mtime = fileModTime(dyStringContents(realFileName));
struct dyString *linkWithTimestamp;

char *scriptName = cgiScriptName();
if (scriptName == NULL)
    scriptName = cloneString("");
boolean nonVersionedLinks = FALSE;
if (endsWith(scriptName, "qaPushQ"))
    nonVersionedLinks = TRUE;
if (nonVersionedLinks)
    linkWithTimestamp = dyStringCreate("%s/%s%s", dyStringContents(fullDirName), baseName, extension);
else if ((cfgOption("versionStamped") == NULL) &&  (hIsPreviewHost() || hIsPrivateHost()))
    linkWithTimestamp = dyStringCreate("%s/%s-%ld%s", dyStringContents(fullDirName), baseName, mtime, extension);
else
    linkWithTimestamp = dyStringCreate("%s/%s-v%s%s", dyStringContents(fullDirName), baseName, CGI_VERSION, extension);

if (!fileExists(dyStringContents(linkWithTimestamp)))
    errAbort("Cannot find correct version of file '%s'; this is due to an installation error\n\nError details: %s does not exist",
             fileName, dyStringContents(linkWithTimestamp));

// Free up all that extra memory
dyStringFree(&realFileName);
dyStringFree(&fullDirName);
char *linkFull = dyStringCannibalize(&linkWithTimestamp);
char *link = linkFull;
if (docRoot != NULL)
    {
    link = cloneString(linkFull + strlen(docRoot) + 1);
    freeMem(linkFull);
    }

if (wrapInHtml) // wrapped for christmas
    {
    struct dyString *wrapped = dyStringNew(0);
    if (js)
        dyStringPrintf(wrapped,"<script type='text/javascript' SRC='/%s'></script>\n", link);
    else if (style)
        dyStringPrintf(wrapped,"<LINK rel='STYLESHEET' href='/%s' TYPE='text/css' />\n", link);
    else // Will be image, since these are the only three choices allowed
        dyStringPrintf(wrapped,"<IMG src='/%s' />\n", link);
    freeMem(link);
    link = dyStringCannibalize(&wrapped);
    }

return link;
}

char *webTimeStampedLinkToResourceOnFirstCall(char *fileName, boolean wrapInHtml)
// If this is the first call, will return full path of timestamped link to the requested
//   resource file (js, or css).  Free after use.
// else returns NULL.  Useful to ensure multiple references to the same resource file are not made
// NOTE: png, jpg and gif should also be supported but are untested.
{
static struct hash *includedResourceFiles = NULL;
if (!includedResourceFiles)
    includedResourceFiles = newHash(0);

if (hashLookup(includedResourceFiles, fileName))
    return NULL;

char * link = webTimeStampedLinkToResource(fileName,wrapInHtml);
if (link)
    hashAdd(includedResourceFiles, fileName, NULL);  // Don't hash link, because
return link;                                         // memory will be freed by caller!!!
}

boolean webIncludeResourcePrintToFile(FILE * toFile, char *fileName)
// Converts fileName to web Resource link and prints the html reference
// This only prints and returns TRUE on first call for this resource.
// Passing in NULL as the file pointer results in hPrintf call
// The reference will be to a link with timestamp.
{
char *link = webTimeStampedLinkToResourceOnFirstCall(fileName,TRUE);
if (link)
    {
    if (toFile == NULL)
        hPrintf("%s",link);
    else
        fprintf(toFile,"%s",link);
    freeMem(link);
    return TRUE;
    }
return FALSE;
}

// overrides for default context specific help link.
char *contextSpecificHelpLink = NULL;
char *contextSpecificHelpLabel = NULL;

void setContextSpecificHelp(char *link, char *label)
// Override default behavior for the context specific help link
{
if(link)
    contextSpecificHelpLink = cloneString(link);
if(label)
    contextSpecificHelpLabel = cloneString(label);
}

char *menuBar(struct cart *cart)
// Return HTML for the menu bar (read from a configuration file);
// we fixup internal CGI's to add hgsid's and include the appropriate js and css files.
{
char *docRoot = hDocumentRoot();
char *menuStr, buf[4096], uiVars[128];
FILE *fd;
int len, offset, err;
char *navBarFile = "inc/globalNavBar.inc";
struct stat statBuf;
regex_t re;
regmatch_t match[2];
char *scriptName = cgiScriptName();
if (cart)
    safef(uiVars, sizeof(uiVars), "%s=%s", cartSessionVarName(), cartSessionId(cart));
else
    uiVars[0] = 0;

if(docRoot == NULL)
    // tolerate missing docRoot (i.e. don't bother with menu when running from command line)
    return NULL;

jsIncludeFile("jquery.js", NULL);
jsIncludeFile("jquery.plugins.js", NULL);
webIncludeResourceFile("nice_menu.css");

// Read in menu bar html
safef(buf, sizeof(buf), "%s/%s", docRoot, navBarFile);
fd = mustOpen(buf, "r");
fstat(fileno(fd), &statBuf);
len = statBuf.st_size;
menuStr = needMem(len + 1);
mustRead(fd, menuStr, statBuf.st_size);
menuStr[len] = 0;
carefulClose(&fd);

if (cart)
    {
    // fixup internal CGIs to have hgsid
    safef(buf, sizeof(buf), "/cgi-bin/hg[A-Za-z]+(%c%c?)", '\\', '?');
    err = regcomp(&re, buf, REG_EXTENDED);
    if(err)
	errAbort("regcomp failed; err: %d", err);
    struct dyString *dy = newDyString(0);
    for(offset = 0; offset < len && !regexec(&re, menuStr + offset, ArraySize(match), match, 0); offset += match[0].rm_eo)
	{
	dyStringAppendN(dy, menuStr + offset, match[0].rm_eo);
	if(match[1].rm_so == match[1].rm_eo)
	    dyStringAppend(dy, "?");
	dyStringAppend(dy, uiVars);
	if(match[1].rm_so != match[1].rm_eo)
	    dyStringAppend(dy, "&");
	}
    if(offset < len)
	dyStringAppend(dy, menuStr + offset);
    freez(&menuStr);
    menuStr = dyStringCannibalize(&dy);
    }
if(!loginSystemEnabled())
    stripRegEx(menuStr, "<\\!-- LOGIN_START -->.*<\\!-- LOGIN_END -->", REG_ICASE);

if(scriptName)
    {  // Provide optional official mirror servers menu items
    char *geoMenu = geoMirrorMenu();
    char *pattern = "<!-- OPTIONAL_MIRROR_MENU -->";
    char *newMenuStr = replaceChars(menuStr, pattern, geoMenu);
    freez(&menuStr);
    menuStr = newMenuStr;
    }


if(scriptName)
    {
    // Provide view menu for some CGIs.
    struct dyString *viewItems = dyStringCreate("");
    boolean hasViewMenu = TRUE;
    if (endsWith(scriptName, "hgGenome"))
        {
	safef(buf, sizeof(buf), "../cgi-bin/hgGenome?%s&hgGenome_doPsOutput=1", uiVars);
    	dyStringPrintf(viewItems, "<li><a href='%s' id='%s'>%s</a></li>\n", buf, "pdfLink", "PDF/PS");
        }
    else
	{
	hasViewMenu = FALSE;
	}
    if (hasViewMenu)
	{
	struct dyString *viewMenu = dyStringCreate("<li class='menuparent' id='view'><span>View</span>\n<ul style='display: none; visibility: hidden;'>\n");
	dyStringAppend(viewMenu, viewItems->string);
	dyStringAppend(viewMenu, "</ul>\n</li>\n");
    	menuStr = replaceChars(menuStr, "<!-- OPTIONAL_VIEW_MENU -->", viewMenu->string);
	dyStringFree(&viewMenu);
	}
    dyStringFree(&viewItems);
    }


if(scriptName)
    {
    // Provide context sensitive help links for some CGIs.
    char *link = NULL;
    char *label = NULL;
    if (endsWith(scriptName, "hgBlat"))
        {
        link = "../goldenPath/help/hgTracksHelp.html#BLATAlign";
        label = "Help on Blat";
        }
    else if (endsWith(scriptName, "hgHubConnect"))
        {
        link = "../goldenPath/help/hgTrackHubHelp.html";
        label = "Help on Track Hubs";
        }
    else if (endsWith(scriptName, "hgNear"))
        {
        link = "../goldenPath/help/hgNearHelp.html";
        label = "Help on Gene Sorter";
        }
    else if (endsWith(scriptName, "hgTables"))
        {
        link = "../goldenPath/help/hgTablesHelp.html";
        label = "Help on Table Browser";
        }
    else if (endsWith(scriptName, "hgGenome"))
        {
        link = "../goldenPath/help/hgGenomeHelp.html";
        label = "Help on Genome Graphs";
        }
    else if (endsWith(scriptName, "hgSession"))
        {
        link = "../goldenPath/help/hgSessionHelp.html";
        label = "Help on Sessions";
        }
    else if (endsWith(scriptName, "hgVisiGene"))
        {
        link = "../goldenPath/help/hgTracksHelp.html#VisiGeneHelp";
        label = "Help on VisiGene";
        }
    else if (endsWith(scriptName, "hgCustom"))
        {
        link = "../goldenPath/help/customTrack.html";
        label = "Help on Custom Tracks";
        }
    // Don't overwrite any previously set defaults
    if(!contextSpecificHelpLink && link)
        contextSpecificHelpLink = link;
    if(!contextSpecificHelpLabel && label)
        contextSpecificHelpLabel = label;
    }
if(contextSpecificHelpLink)
    {
    char buf[1024];
    safef(buf, sizeof(buf), "<li><a href='%s'>%s</a></li>", contextSpecificHelpLink, contextSpecificHelpLabel);
    menuStr = replaceChars(menuStr, "<!-- CONTEXT_SPECIFIC_HELP -->", buf);
    }
return menuStr;
}
