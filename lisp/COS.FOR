      SUBROUTINE PUTCH(VEC,CH,I)
      INTEGER VEC(*),BI,CH,SHFT,MSK,WI,WORD

      WI   = SHIFTR(I - 1, 3) + 1
      BI   = (I - 1).AND.7
      WORD = VEC(WI)
      MSK  = MASK(8)
      SHFT = SHIFTL(BI, 3)
      VEC(WI) = (WORD.AND.(.NOT.SHIFTR(MSK, SHFT)))
     &          .OR.SHIFTL(CH, 56 - SHFT)
      END
      SUBROUTINE GETCH(VEC,CH,I)
      INTEGER VEC(*),BI,CH,WI

      WI = SHIFTR(I - 1, 3) + 1
      BI = (I - 1).AND.7
      CH = SHIFTR(VEC(WI), 56 - SHIFTL(BI, 3)).AND.255
      END
      FUNCTION OPENF(X)
C-- OPENS AN OS-FILE. RETURNS LOGICAL UNIT.
C-- IF OPENF LE 0 THEN ERROR
C-- X IS A LIST (FILENAME INPUT . BINARY)
C-- IF INPUT NE NIL   INPUT FILE (ELSE OUTPUT)
C-- IF BINARY NE NIL  BINARY FILE (ELSE ASCII)
      INCLUDE 'F4COM.FOR'
      COMMON /FILTAB/ LUNTAB(10)
      INTEGER OPENF,UN,X
      CHARACTER*3 ACC
      CHARACTER*11 MODE
      CHARACTER*7 FNAME
      EQUIVALENCE (FNAME,ABUFF(1))

      OPENF=-1
C-- IF NLISTP(X) THEN GOTO 3
      IF(X.LE.NATOM.OR.X.GT.NFREET) THEN
        WRITE(103,*) 'OPENF invalid parameters'
        RETURN
      ENDIF
      IFILE=CAR(X)
      X=CDR(X)
      IF(X.LE.NATOM.OR.X.GT.NFREET) THEN
        WRITE(103,*) 'OPENF invalid parameters'
        RETURN
      ENDIF
      INPUT=CAR(X)
      IBIN=CDR(X)
      IF(MPNAME(IFILE,ABUFF(1),7,IPL).NE.0) THEN
        WRITE(103,*) 'Invalid file name: ',FNAME
        RETURN
      ENDIF
      DO I=1,10
        IF(LUNTAB(I).EQ.0)GO TO 1
      END DO
      WRITE(103,*) 'LUN table full. Failed to open ',FNAME
      RETURN

1     CONTINUE
      LUNTAB(I)=1
C-- WE USE LOGICAL UNIT 10-19
      UN=I+9
      IF(INPUT.NE.NIL) THEN
        ACC='OLD'
      ELSE
        ACC='NEW'
      ENDIF
      IF(IBIN.NE.NIL) THEN
        MODE='UNFORMATTED'
      ELSE
        MODE='FORMATTED'
      ENDIF
      OPEN(FILE=FNAME(1:IPL),UNIT=UN,FORM=MODE,STATUS=ACC,
     &     ERR=2,IOSTAT=IOS)
      OPENF=UN
      RETURN

2     CONTINUE
      WRITE(103,*) 'Error ',IOS,'. Failed to open ',FNAME
      END
      SUBROUTINE REW(LUN)
      COMMON /FILTAB/ LUNTAB(10)
      CLOSE(UNIT=LUN,ERR=999,IOSTAT=IOS)
      IF( LUN.GE.10 .AND. LUN.LT.20 ) LUNTAB(LUN-9)=0
      RETURN
999   WRITE(103,*) 'Error ',IOS,'. REW failed to close unit ',LUN
      END
      SUBROUTINE EJECT(LUN)
      WRITE(LUN,*) '1'
      END
      FUNCTION MSLFT(I)
      MSLFT=0
      END
      SUBROUTINE MTIME(IT)
      IT = CLOCK()
      END
      SUBROUTINE MDATE(IT)
C SHOULD RETURN 10 CHARACTERS
      IT = DATE()
      END
      SUBROUTINE BRSET
      RETURN
      END
      SUBROUTINE UPCASE(BUFF,L)
      INTEGER BUFF(*),L
C                           UPPERCASE THE CHARACTERS IN ABUFF
C                           NUMBER OF CHARACTERS = L
C                           FORMAT IS A1 FORMAT
      DO I = 1,L
        IC = BUFF(I)
        IF (IC.GE.97.AND.IC.LE.122) THEN
          BUFF(I) = IC - 32
        END IF
      END DO
      END
