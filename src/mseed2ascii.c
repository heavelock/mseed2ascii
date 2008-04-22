/***************************************************************************
 * mseed2ascii.c
 *
 * Convert Mini-SEED waveform data to ASCII
 *
 * Written by Chad Trabant, IRIS Data Management Center
 *
 * modified 2008.112
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <math.h>

#include <libmseed.h>

#define VERSION "0.1"
#define PACKAGE "mseed2ascii"

struct listnode {
  char *key;
  char *data;
  struct listnode *next;
};

static int writeascii (MSTrace *mst);
static int writebinarysac (struct SACHeader *sh, float *fdata, int npts,
			   char *outfile);
static int writealphasac (struct SACHeader *sh, float *fdata, int npts,
			  char *outfile);
static int parameter_proc (int argcount, char **argvec);
static char *getoptval (int argcount, char **argvec, int argopt, int dasharg);
static int readlistfile (char *listfile);
static struct listnode *addnode (struct listnode **listroot, void *key, int keylen,
				 void *data, int datalen);
static void usage (void);

static int   verbose      = 0;
static int   reclen       = -1;
static int   indifile     = 0;
static int   reconstruct  = 0;
static int   outformat    = 1;

/* A list of input files */
struct listnode *filelist = 0;


int
main (int argc, char **argv)
{
  MSTraceGroup *mstg = 0;
  MSTrace *mst;
  MSRecord *msr = 0;
  
  struct listnode *flp;
  
  int retcode;
  int totalrecs = 0;
  int totalsamps = 0;
  int totalfiles = 0;
  
  /* Process given parameters (command line and parameter file) */
  if (parameter_proc (argc, argv) < 0)
    return -1;
  
  /* Init MSTraceGroup */
  mstg = mst_initgroup (mstg);
  
  /* Read input miniSEED files into MSTraceGroup */
  flp = filelist;
  while ( flp != 0 )
    {
      if ( verbose )
        fprintf (stderr, "Reading %s\n", flp->data);
      
      while ( (retcode = ms_readmsr(&msr, flp->data, reclen, NULL, NULL,
				    1, 1, verbose-1)) == MS_NOERROR )
	{
	  if ( verbose > 1)
	    msr_print (msr, verbose - 2);
	  
	  mst_addmsrtogroup (mstg, msr, 1, -1.0, -1.0);
	  
	  totalrecs++;
	  totalsamps += msr->samplecnt;
	}
      
      if ( retcode != MS_ENDOFFILE )
	fprintf (stderr, "Error reading %s: %s\n", flp->data, ms_errorstr(retcode));
      
      /* Make sure everything is cleaned up */
      ms_readmsr (&msr, NULL, 0, NULL, NULL, 0, 0, 0);
      
      /* If processing each file individually, write ASCII and reset */
      if ( indifile )
	{
	  mst = mstg->traces;
	  while ( mst )
	    {
	      writeascii (mst);
	      mst = mst->next;
	    }
	  
	  mstg = mst_initgroup (mstg);
	}
      
      totalfiles++;
      flp = flp->next;
    }
  
  if ( ! indifile )
    {
      mst = mstg->traces;
      while ( mst )
	{
	  writeascii (mst);
	  mst = mst->next;
	}
    }
  
  /* Make sure everything is cleaned up */
  mst_freegroup (&mstg);
  
  if ( verbose )
    printf ("Files: %d, Records: %d, Samples: %d\n", totalfiles, totalrecs, totalsamps);
  
  return 0;
}  /* End of main() */


/***************************************************************************
 * writeascii:
 * 
 * Write data buffer to output file as ASCII.
 *
 * Returns the number of samples written or -1 on error.
 ***************************************************************************/
static int
writeascii (MSTrace *mst)
{
  char outfile[1024];
  char timestr[60];
  
  float *fdata = 0;
  double *ddata = 0;
  int32_t *idata = 0;
  hptime_t submsec;
  int idx;
  
  if ( ! mst )
    return -1;
  
  if ( mst->numsamples == 0 || mst->samprate == 0.0 )
    return 0;
  
  if ( verbose )
    fprintf (stderr, "Writing ASCII for %.8s.%.8s.%.8s.%.8s\n",
	     mst->network, mst->station, mst->location, mst->channel);
  
  /* Generate ISO timestr */
  ms_hptimeisotimestr (mst->starttime, timestr, 1);
  
  /* Create output file name: Net.Sta.Loc.Chan.Qual.Year/Month/DayTHour:Min:Sec.Subsec */
  snprintf (outfile, sizeof(outfile), "%s.%s.%s.%s.%c.%s",
	    mst->network, mst->station, mst->location, mst->channel,
	    mst->dataquality, timestr);
  
  /* Open file */
  // Open file

  /* Print header line: "Net_Sta_Loc_Chan_Qual, ## samples, ## sps, isotime, INTEGER|FLOAT, Units" */
  // print header
  
  // print samples:

  /* Convert data buffer to floats */
  if ( mst->sampletype == 'f' )
    {
      fdata = (float *) mst->datasamples;

      for (idx=0; idx < mst->numsamples; idx++)
	fdata[idx] = (float) idata[idx];
    }
  else if ( mst->sampletype == 'i' )
    {
      idata = (int32_t *) mst->datasamples;
      
      for (idx=0; idx < mst->numsamples; idx++)
	fdata[idx] = (float) idata[idx];
    }
  else if ( mst->sampletype == 'd' )
    {
      ddata = (double *) mst->datasamples;

      for (idx=0; idx < mst->numsamples; idx++)
	fdata[idx] = (float) ddata[idx];
    }
  else
    {
      fprintf (stderr, "Error, unrecognized sample type: '%c'\n",
	       mst->sampletype);
      return -1;
    }
  
  if ( outformat == 1 )
    {
      if ( verbose > 1 )
	fprintf (stderr, "Writing binary SAC file: %s\n", outfile);
      
      if ( writebinarysac (&sh, fdata, mst->numsamples, outfile) )
	return -1;
    }
  else if ( outformat == 2 )
    {
      if ( verbose > 1 )
	fprintf (stderr, "Writing alphanumeric SAC file: %s\n", outfile);
      
      if ( writealphasac (&sh, fdata, mst->numsamples, outfile) )
	return -1;
    }
  else
    {
      fprintf (stderr, "Error, unrecognized format: '%d'\n", outformat);
    }
  
  // close file
  
  fprintf (stderr, "Wrote %d samples to %s\n", mst->numsamples, outfile);
  
  return mst->numsamples;
}  /* End of writesac() */


/***************************************************************************
 * writebinarysac:
 * Write binary SAC file.
 *
 * Returns 0 on success, and -1 on failure.
 ***************************************************************************/
static int
writebinarysac (struct SACHeader *sh, float *fdata, int npts, char *outfile)
{
  FILE *ofp;
  
  /* Open output file */
  if ( (ofp = fopen (outfile, "wb")) == NULL )
    {
      fprintf (stderr, "Cannot open output file: %s (%s)\n",
	       outfile, strerror(errno));
      return -1;
    }
  
  /* Write SAC header to output file */
  if ( fwrite (sh, sizeof(struct SACHeader), 1, ofp) != 1 )
    {
      fprintf (stderr, "Error writing SAC header to output file\n");
      return -1;
    }
  
  /* Write float data to output file */
  if ( fwrite (fdata, sizeof(float), npts, ofp) != npts )
    {
      fprintf (stderr, "Error writing SAC data to output file\n");
      return -1;
    }
  
  fclose (ofp);
  
  return 0;
}  /* End of writebinarysac() */


/***************************************************************************
 * writealphasac:
 * Write alphanumeric SAC file.
 *
 * Returns 0 on success, and -1 on failure.
 ***************************************************************************/
static int
writealphasac (struct SACHeader *sh, float *fdata, int npts, char *outfile)
{
  FILE *ofp;
  int idx, fidx;
  
  /* Declare and set up pointers to header variable type sections */
  float   *fhp = (float *) sh;
  int32_t *ihp = (int32_t *) sh + (NUMFLOATHDR);
  char    *shp = (char *) sh + (NUMFLOATHDR * 4 + NUMINTHDR * 4);
  
  /* Open output file */
  if ( (ofp = fopen (outfile, "wb")) == NULL )
    {
      fprintf (stderr, "Cannot open output file: %s (%s)\n",
	       outfile, strerror(errno));
      return -1;
    }
  
  /* Write SAC header float variables to output file, 5 variables per line */
  for (idx=0; idx < NUMFLOATHDR; idx += 5)
    {
      for (fidx=idx; fidx < (idx+5) && fidx < NUMFLOATHDR; fidx++)
	fprintf (ofp, "%#15.7g", *(fhp + fidx));
      
      fprintf (ofp, "\n");
    }
  
  /* Write SAC header integer variables to output file, 5 variables per line */
  for (idx=0; idx < NUMINTHDR; idx += 5)
    {
      for (fidx=idx; fidx < (idx+5) && fidx < NUMINTHDR; fidx++)
	fprintf (ofp, "%10d", *(ihp + fidx));
      
      fprintf (ofp, "\n");
    }
  
  /* Write SAC header string variables to output file, 3 variables per line */
  for (idx=0; idx < NUMSTRHDR; idx += 3)
    {
      if ( idx == 0 )
	fprintf (ofp, "%-8.8s%-16.16s", shp, shp + 8);
      else
	fprintf (ofp, "%-8.8s%-8.8s%-8.8s", shp+(idx*8), shp+((idx+1)*8), shp+((idx+2)*8));
      
      fprintf (ofp, "\n");
    }
  
  /* Write float data to output file, 5 values per line */
  for (idx=0; idx < npts; idx += 5)
    {
      for (fidx=idx; fidx < (idx+5) && fidx < npts && fidx >= 0; fidx++)
	fprintf (ofp, "%#15.7g", *(fdata + fidx));
      
      fprintf (ofp, "\n");
    }
  
  fclose (ofp);
  
  return 0;
}  /* End of writealphasac() */


/***************************************************************************
 * parameter_proc:
 * Process the command line parameters.
 *
 * Returns 0 on success, and -1 on failure.
 ***************************************************************************/
static int
parameter_proc (int argcount, char **argvec)
{
  int optind;
  
  /* Process all command line arguments */
  for (optind = 1; optind < argcount; optind++)
    {
      if (strcmp (argvec[optind], "-V") == 0)
	{
	  fprintf (stderr, "%s version: %s\n", PACKAGE, VERSION);
	  exit (0);
	}
      else if (strcmp (argvec[optind], "-h") == 0)
	{
	  usage();
	  exit (0);
	}
      else if (strncmp (argvec[optind], "-v", 2) == 0)
	{
	  verbose += strspn (&argvec[optind][1], "v");
	}
      else if (strcmp (argvec[optind], "-r") == 0)
	{
	  reclen = strtoul (getoptval(argcount, argvec, optind++, 0), NULL, 10);
	}
      else if (strcmp (argvec[optind], "-i") == 0)
	{
	  indifile = 1;
	}
      else if (strcmp (argvec[optind], "-R") == 0)
	{
	  reconstruct = 1;
	}
      else if (strcmp (argvec[optind], "-f") == 0)
	{
	  outformat = strtoul (getoptval(argcount, argvec, optind++, 0), NULL, 10);
	}
      else if (strncmp (argvec[optind], "-", 1) == 0 &&
               strlen (argvec[optind]) > 1 )
        {
          fprintf(stderr, "Unknown option: %s\n", argvec[optind]);
          exit (1);
        }
      else
        {
	  /* Add the file name to the intput file list */
          if ( ! addnode (&filelist, NULL, 0, argvec[optind], strlen(argvec[optind])+1) )
	    {
	      fprintf (stderr, "Error adding file name to list\n");
	    }
        }
    }
  
  /* Make sure an input files were specified */
  if ( filelist == 0 )
    {
      fprintf (stderr, "No input files were specified\n\n");
      fprintf (stderr, "%s version %s\n\n", PACKAGE, VERSION);
      fprintf (stderr, "Try %s -h for usage\n", PACKAGE);
      exit (1);
    }
  
  /* Report the program version */
  if ( verbose )
    fprintf (stderr, "%s version: %s\n", PACKAGE, VERSION);
  
  /* Check the input files for any list files, if any are found
   * remove them from the list and add the contained list */
  if ( filelist )
    {
      struct listnode *prevln, *ln;
      char *lfname;

      prevln = ln = filelist;
      while ( ln != 0 )
        {
          lfname = ln->data;

          if ( *lfname == '@' )
            {
              /* Remove this node from the list */
              if ( ln == filelist )
                filelist = ln->next;
              else
                prevln->next = ln->next;

              /* Skip the '@' first character */
              if ( *lfname == '@' )
                lfname++;

              /* Read list file */
              readlistfile (lfname);

              /* Free memory for this node */
              if ( ln->key )
                free (ln->key);
              free (ln->data);
              free (ln);
            }
          else
            {
              prevln = ln;
            }
	  
          ln = ln->next;
        }
    }
  
  return 0;
}  /* End of parameter_proc() */


/***************************************************************************
 * getoptval:
 * Return the value to a command line option; checking that the value is 
 * itself not an option (starting with '-') and is not past the end of
 * the argument list.
 *
 * argcount: total arguments in argvec
 * argvec: argument list
 * argopt: index of option to process, value is expected to be at argopt+1
 *
 * Returns value on success and exits with error message on failure
 ***************************************************************************/
static char *
getoptval (int argcount, char **argvec, int argopt, int dasharg)
{
  if ( argvec == NULL || argvec[argopt] == NULL ) {
    fprintf (stderr, "getoptval(): NULL option requested\n");
    exit (1);
    return 0;
  }
  
  /* When the value potentially starts with a dash (-) */
  if ( (argopt+1) < argcount && dasharg )
    return argvec[argopt+1];
  
  /* Otherwise check that the value is not another option */
  if ( (argopt+1) < argcount && *argvec[argopt+1] != '-' )
    return argvec[argopt+1];
  
  fprintf (stderr, "Option %s requires a value\n", argvec[argopt]);
  exit (1);
  return 0;
}  /* End of getoptval() */


/***************************************************************************
 * readlistfile:
 *
 * Read a list of files from a file and add them to the filelist for
 * input data.  The filename is expected to be the last
 * space-separated field on the line.
 *
 * Returns the number of file names parsed from the list or -1 on error.
 ***************************************************************************/
static int
readlistfile (char *listfile)
{
  FILE *fp;
  char  line[1024];
  char *ptr;
  int   filecnt = 0;
  
  char  filename[1024];
  char *lastfield = 0;
  int   fields = 0;
  int   wspace;
  
  /* Open the list file */
  if ( (fp = fopen (listfile, "rb")) == NULL )
    {
      if (errno == ENOENT)
        {
          fprintf (stderr, "Could not find list file %s\n", listfile);
          return -1;
        }
      else
        {
          fprintf (stderr, "Error opening list file %s: %s\n",
                   listfile, strerror (errno));
          return -1;
        }
    }
  if ( verbose )
    fprintf (stderr, "Reading list of input files from %s\n", listfile);

  while ( (fgets (line, sizeof(line), fp)) !=  NULL)
    {
      /* Truncate line at first \r or \n, count space-separated fields
       * and track last field */
      fields = 0;
      wspace = 0;
      ptr = line;
      while ( *ptr )
        {
          if ( *ptr == '\r' || *ptr == '\n' || *ptr == '\0' )
            {
              *ptr = '\0';
              break;
            }
          else if ( *ptr != ' ' )
            {
              if ( wspace || ptr == line )
                {
                  fields++; lastfield = ptr;
                }
              wspace = 0;
            }
          else
            {
              wspace = 1;
            }

          ptr++;
        }

      /* Skip empty lines */
      if ( ! lastfield )
        continue;

      if ( fields >= 1 && fields <= 3 )
        {
          fields = sscanf (lastfield, "%s", filename);

          if ( fields != 1 )
            {
              fprintf (stderr, "Error parsing file name from: %s\n", line);
              continue;
            }

          if ( verbose > 1 )
            fprintf (stderr, "Adding '%s' to input file list\n", filename);

	  /* Add file name to the intput file list */
	  if ( ! addnode (&filelist, NULL, 0, filename, strlen(filename)+1) )
	    {
	      fprintf (stderr, "Error adding file name to list\n");
	    }
	  
          filecnt++;

          continue;
        }
    }

  fclose (fp);

  return filecnt;
}  /* End readlistfile() */


/***************************************************************************
 * addnode:
 *
 * Add node to the specified list.
 *
 * Return a pointer to the added node on success and NULL on error.
 ***************************************************************************/
static struct listnode *
addnode (struct listnode **listroot, void *key, int keylen,
	 void *data, int datalen)
{
  struct listnode *lastlp, *newlp;
  
  if ( data == NULL )
    {
      fprintf (stderr, "addnode(): No data specified\n");
      return NULL;
    }
  
  lastlp = *listroot;
  while ( lastlp != 0 )
    {
      if ( lastlp->next == 0 )
        break;

      lastlp = lastlp->next;
    }

  /* Create new listnode */
  newlp = (struct listnode *) malloc (sizeof (struct listnode));
  memset (newlp, 0, sizeof (struct listnode));
  
  if ( key )
    {
      newlp->key = malloc (keylen);
      memcpy (newlp->key, key, keylen);
    }
  
  if ( data)
    {
      newlp->data = malloc (datalen);
      memcpy (newlp->data, data, datalen);
    }
  
  newlp->next = 0;
  
  if ( lastlp == 0 )
    *listroot = newlp;
  else
    lastlp->next = newlp;
  
  return newlp;
}  /* End of addnode() */


/***************************************************************************
 * usage:
 * Print the usage message and exit.
 ***************************************************************************/
static void
usage (void)
{
  fprintf (stderr, "%s version: %s\n\n", PACKAGE, VERSION);
  fprintf (stderr, "Convert Mini-SEED data to ASCII\n\n");
  fprintf (stderr, "Usage: %s [options] input1.mseed [input2.mseed ...]\n\n", PACKAGE);
  fprintf (stderr,
	   " ## Options ##\n"
	   " -V             Report program version\n"
	   " -h             Show this usage message\n"
	   " -v             Be more verbose, multiple flags can be used\n"
	   " -r bytes       Specify SEED record length in bytes, default: 4096\n"
	   " -i             Process each input file individually instead of merged\n"
	   " -R             Reconstruct time-series across SEED data records\n"
	   " -f format      Specify ASCII output format (default is 1):\n"
           "                  1=Header followed by sample values\n"
           "                  2=Header followed by time-sample value pairs\n"
	   "\n");
}  /* End of usage() */