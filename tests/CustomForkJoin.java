

class CustomForkJoin {
	static final int SIZE = 1000;
   static       int dum  = 0;
  public static void main(String args[]) {
    long t1, t2;

    int[][] a, b, c, c2;
    int NTHREADS = Integer.parseInt(args[0]);
    a  = new int[SIZE][SIZE];
    b  = new int[SIZE][SIZE];
    c  = new int[SIZE][SIZE];
    c2 = new int[SIZE][SIZE];

    // INIT

    for (int i = 0; i < SIZE; i++) 
      for (int j = 0; j < SIZE; j++) {
	a[i][j]  = 1;
	b[i][j]  = 2;
	dum++; // prevents parallelization
      }


      t1 = System.currentTimeMillis();

			Thread[] threads = new Thread[NTHREADS];	
			for (int i = 0; i < NTHREADS ; i++) {
				final int lb = i * SIZE/NTHREADS;
				final int ub = (i+1) * SIZE/NTHREADS;
				threads[i] = new Thread(new Runnable() {

					public void run() {
						for (int i = lb; i < ub; i++)
							for (int j = 0; j < SIZE; j++) {
								c2[i][j] = 0;
								for (int k = 0; k < SIZE; k++)
									c2[i][j] += a[i][k] * b[k][j];
							}
					}
				});
				threads[i].start();
			}
			
			// wait for completion
			for (int i = 0; i < NTHREADS; i++) {
				try {
					threads[i].join();
				} catch (InterruptedException ignore) {
				}
			}

      t2 = System.currentTimeMillis();
      double d = (t2-t1) / 1000.0d;
      System.out.print(d);




    }
}
