BEGIN {s=0}

s==0 && $1=="#" && $2==name {s=1;next}

s==1 && NF==0 {s=2; next}

s==2 && NF==0 {s=3; next}

(s==0 || s==3) && NF==1 && $1=="NEW" {
      printf("\n# %s %s\n\n", name, date);
      s=4;
      next;
    }

s==0 || s==3 || s==4 {print}

