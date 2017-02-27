

class Threading {
	static final int SIZE = 1000;
   static       int dum  = 0;
  public static void main(String args[]) {

    long t1, t2;

    byte[][] a, b, c, c2;

    a  = new byte[SIZE][SIZE];
    b  = new byte[SIZE][SIZE];
    c  = new byte[SIZE][SIZE];
    c2 = new byte[SIZE][SIZE];

    // INIT

    for (int i = 0; i < SIZE; i++) 
      for (int j = 0; j < SIZE; j++) {
	a[i][j]  = 1;
	b[i][j]  = 2;
	dum++; // prevents parallelization
      }


      t1 = System.currentTimeMillis();

  /*par */
    for (int i = 0; i < SIZE; i++) 
      for (int j = 0; j < SIZE; j++)  {
	c2[i][j] = 0;
	for (int k = 0; k < SIZE; k++)  
	   c2[i][j] += a[i][k] * b[k][j];
      }

      t2 = System.currentTimeMillis();
      double d = (t2-t1) / 1000.0d;
      System.out.print(d);

    }
}
