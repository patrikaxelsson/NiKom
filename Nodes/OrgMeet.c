#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "NiKomstr.h"
#include "NiKomFuncs.h"
#include "NiKomLib.h"
#include "Logging.h"
#include "Stack.h"
#include "Terminal.h"
#include "KOM.h"

#include "OrgMeet.h"

#define EKO		1
#define KOM		1
#define EJKOM	0

extern struct System *Servermem;
extern char outbuffer[],*argument,inmat[];
extern int nodnr,senast_text_typ,senast_text_nr,senast_text_mote,nu_skrivs,inloggad,
	rad,mote2;
extern struct Header readhead,sparhead;
extern struct Inloggning Statstr;
extern struct MinList edit_list;

int org_skriv(void) {
	int editret;
	if(org_initheader(EJKOM)) return(1);
	nu_skrivs=TEXT;
	if((editret=edittext(NULL))==1) return(1);
	if(!editret) org_sparatext();
	return(0);
}

int org_kommentera(void) {
  int nummer, editret, confId, isCorrect;
  struct Mote *motpek;
  nummer=atoi(argument);
  if(argument[0]) {
    if(nummer<Servermem->info.lowtext || nummer>Servermem->info.hightext) {
      puttekn("\r\n\nTexten finns inte!\r\n\n",-1);
      return(0);
    }
    confId = GetConferenceForText(nummer);
    if(!MayBeMemberConf(confId, inloggad, &Servermem->inne[nodnr])) {
      puttekn("\r\n\nDu har inga r�ttigheter i m�tet d�r texten finns!\r\n\n",-1);
      return(0);
    }
    motpek=getmotpek(confId);
    if(!motpek) {
      puttekn("\n\n\rHmm.. M�tet d�r texten ligger finns inte..\n\r",-1);
      return(0);
    }
    if(motpek->status & KOMSKYDD) {
      if(!MayReplyConf(motpek->nummer, inloggad, &Servermem->inne[nodnr])) {
        puttekn("\r\n\nDu f�r inte kommentera i kommentarsskyddade m�ten!\r\n\n",-1);
        return(0);
      } else {
        if(GetYesOrNo(
           "\r\n\nVill du verkligen kommentera i ett kommentarsskyddat m�te? ",
           'j', 'n', "Ja\r\n", "Nej\r\n", FALSE, &isCorrect)) {
          return 1;
        }
        if(!isCorrect) {
          return 0;
        }
      }
    }
    if(readtexthead(nummer,&readhead)) return(0);
    senast_text_typ=TEXT;
    senast_text_nr=nummer;
    senast_text_mote=motpek->nummer;
  }
  motpek=getmotpek(senast_text_mote);
  if(!motpek) {
    puttekn("\n\n\rHmm.. M�tet d�r texten ligger finns inte..\n\r",-1);
    return(0);
  }
  if(org_initheader(KOM)) return(1);
  nu_skrivs=TEXT;
  if((editret=edittext(NULL))==1) return(1);
  else if(!editret)	{
    org_linkkom();
    org_sparatext();
  }
  return(0);
}

void org_lasa(int tnr) {
	if(tnr<Servermem->info.lowtext || tnr>Servermem->info.hightext) {
		puttekn("\r\n\nTexten finns inte!\r\n",-1);
		return;
	}
	org_visatext(tnr);
}

int HasUnreadInOrgConf(int conf) {
  long unreadText;

  unreadText = FindNextUnreadText(0, conf, &Servermem->unreadTexts[nodnr]);
  if(unreadText == -1) {
    Servermem->unreadTexts[nodnr].lowestPossibleUnreadText[conf] =
      Servermem->info.hightext + 1;
  }
  return unreadText != -1;
}

void pushTextRepliesToStack(struct Header *textHeader) {
  int i, confId, textId;

  for(i = MAXKOM; i >= 0; i--) {
    if(textHeader->kom_i[i] == -1) {
      continue;
    }
    textId = textHeader->kom_i[i];
    confId = GetConferenceForText(textId);
    if(confId == -1
       || !IsTextUnread(textId, &Servermem->unreadTexts[nodnr])
       || !IsMemberConf(confId, inloggad, &Servermem->inne[nodnr])) {
      continue;
    }
    StackPush(g_unreadRepliesStack, textId);
  }
}

void displayTextAndClearUnread(int textId) {
  ChangeUnreadTextStatus(textId, 0, &Servermem->unreadTexts[nodnr]);
  if(org_visatext(textId)) {
    pushTextRepliesToStack(&readhead);
  }
}

void NextTextInOrgConf(void) {
  int textId;

  textId = FindNextUnreadText(0, mote2, &Servermem->unreadTexts[nodnr]);
  if(textId == -1) {
    SendString("\n\n\rFinns inga ol�sta texter i detta m�te.\n\r");
    return;
  }
  StackClear(g_unreadRepliesStack);
  displayTextAndClearUnread(textId);
}

void NextReplyInOrgConf(void) {
  if(StackSize(g_unreadRepliesStack) == 0) {
    SendString("\n\n\rFinns inga fler ol�sta kommentarer\n\r");
    return;
  }
  displayTextAndClearUnread(StackPop(g_unreadRepliesStack));
}

int org_visatext(int text) {
  int x,length, confId;
  struct tm *ts;
  struct EditLine *el;
  Servermem->inne[nodnr].read++;
  Servermem->info.lasta++;
  Statstr.read++;
  if(GetConferenceForText(text) == -1) {
    sprintf(outbuffer,"\n\n\rText %d �r raderad!\n\n\r",text);
    puttekn(outbuffer,-1);
    if(Servermem->inne[nodnr].status<Servermem->cfg.st.medmoten) return(0);
  }
  if(readtexthead(text,&readhead)) {
    return(0);
  }
  if(!MayReadConf(readhead.mote, inloggad, &Servermem->inne[nodnr])) {
    puttekn("\r\n\nDu har inte r�tt att l�sa den texten!\r\n\n",-1);
    return(0);
  }
  ts=localtime(&readhead.tid);
  sprintf(outbuffer,"\r\n\nText %d  M�te: %s    %4d%02d%02d %02d:%02d\r\n",
          readhead.nummer, getmotnamn(readhead.mote),
          ts->tm_year + 1900, ts->tm_mon + 1, ts->tm_mday, ts->tm_hour,
          ts->tm_min);
  puttekn(outbuffer,-1);
  sprintf(outbuffer,"Skriven av %s\r\n", readhead.person !=-1
          ? getusername(readhead.person) : "<raderad anv�ndare>");
  puttekn(outbuffer,-1);
  if(readhead.kom_till_nr!=-1) {
    sprintf(outbuffer,"Kommentar till text %d av %s\r\n", readhead.kom_till_nr,
            getusername(readhead.kom_till_per));
    puttekn(outbuffer,-1);
  }
  sprintf(outbuffer,"�rende: %s\r\n",readhead.arende);
  puttekn(outbuffer,-1);
  if(Servermem->inne[nodnr].flaggor & STRECKRAD) {
    length=strlen(outbuffer);
    for(x=0;x<length-2;x++) outbuffer[x]='-';
    outbuffer[x]=0;
    puttekn(outbuffer,-1);
    puttekn("\r\n\n",-1);
  } else puttekn("\n",-1);
  if(readtextlines(TEXT,readhead.textoffset,readhead.rader,readhead.nummer))
    puttekn("\n\rFel vid l�sandet i Text.dat\n\r",-1);
  for(el=(struct EditLine *)edit_list.mlh_Head;
      el->line_node.mln_Succ;
      el=(struct EditLine *)el->line_node.mln_Succ) {
    if(puttekn(el->text,-1)) break;
    eka('\r');
  }
  freeeditlist();
  sprintf(outbuffer, "\n(Slut p� text %d av %s)\r\n", readhead.nummer,
          getusername(readhead.person));
  puttekn(outbuffer,-1);
  x=0;
  while(readhead.kom_i[x]!=-1) {
    confId = GetConferenceForText(readhead.kom_i[x]);
    if(confId != -1 && IsMemberConf(confId, inloggad, &Servermem->inne[nodnr])) {
      sprintf(outbuffer, "  (Kommentar i text %d av %s)\r\n", readhead.kom_i[x],
              getusername(readhead.kom_av[x]));
      puttekn(outbuffer,-1);
    }
    x++;
  }
  senast_text_typ=TEXT;
  senast_text_nr=readhead.nummer;
  senast_text_mote=readhead.mote;
  if(readhead.kom_i[0]!=-1) {
    return(1);
  }
  return(0);
}

void org_sparatext(void) {
	int x, nummer;
	Servermem->inne[nodnr].skrivit++;
	Servermem->info.skrivna++;
	Statstr.write++;
	sparhead.rader=rad;
	for(x=0;x<MAXKOM;x++) {
		sparhead.kom_i[x]=-1;
		sparhead.kom_av[x]=-1;
	}
	nummer = sendservermess(SPARATEXTEN,(long)&sparhead);
	sprintf(outbuffer,"\r\nTexten fick nummer %d\r\n",nummer);
	puttekn(outbuffer,-1);
	if(Servermem->cfg.logmask & LOG_TEXT) {
          LogEvent(USAGE_LOG, INFO, "%s skriver text %d i %s",
                   getusername(inloggad), nummer, getmotnamn(sparhead.mote));
	}
	freeeditlist();
}

void org_linkkom(void) {
	int x=0;
	struct Header linkhead;
	if(readtexthead(readhead.nummer,&linkhead)) return;
	while(linkhead.kom_i[x]!=-1 && x<MAXKOM) x++;
	if(x==MAXKOM) {
		puttekn("\r\n\nRedan maximalt med kommentarer till den texten, sparar texten i alla fall.\r\n\n",-1);
		return;
	}
	linkhead.kom_i[x]=Servermem->info.hightext+1;
	linkhead.kom_av[x]=sparhead.person;
	writetexthead(readhead.nummer,&linkhead);
}

int org_initheader(int komm) {
	int length=0,x=0;
	long tid;
	struct tm *ts;
	sparhead.person=inloggad;
	if(komm) {
          sparhead.kom_till_nr = readhead.nummer;
          sparhead.kom_till_per = readhead.person;
          sparhead.mote = readhead.mote;
          sparhead.root_text = readhead.root_text;
	} else {
          sparhead.kom_till_nr = -1;
          sparhead.kom_till_per = -1;
          sparhead.mote = mote2;
          sparhead.root_text = 0;
	}
	Servermem->action[nodnr] = SKRIVER;
	Servermem->varmote[nodnr] = sparhead.mote;
	time(&tid);
	ts=localtime(&tid);
	sparhead.tid=tid;
	sparhead.textoffset=(long)&edit_list;
	sparhead.status=0;
	sprintf(outbuffer,"\r\n\nM�te: %s    %4d%02d%02d %02d:%02d\r\n",
                getmotnamn(sparhead.mote), ts->tm_year + 1900, ts->tm_mon + 1,
                ts->tm_mday, ts->tm_hour, ts->tm_min);
	puttekn(outbuffer,-1);
	sprintf(outbuffer,"Skriven av %s\r\n",getusername(inloggad));
	puttekn(outbuffer,-1);
	if(komm) {
		sprintf(outbuffer,"Kommentar till text %d av %s\r\n",sparhead.kom_till_nr,getusername(sparhead.kom_till_per));
		puttekn(outbuffer,-1);
	}
	puttekn("�rende: ",-1);
	if(komm) {
		strcpy(sparhead.arende,readhead.arende);
		puttekn(sparhead.arende,-1);
	} else {
		if(getstring(EKO,40,NULL)) return(1);
		strcpy(sparhead.arende,inmat);
	}
	puttekn("\r\n",-1);
	if(Servermem->inne[nodnr].flaggor & STRECKRAD) {
		length=strlen(sparhead.arende);
		for(x=0;x<length+8;x++) outbuffer[x]='-';
		outbuffer[x]=0;
		puttekn(outbuffer,-1);
		puttekn("\r\n\n",-1);
	} else puttekn("\n",-1);
	return(0);
}

void org_endast(int conf,int amount) {
  SetUnreadTexts(conf, amount, &Servermem->unreadTexts[nodnr]);
}
