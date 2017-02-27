class Par_test {
	static final int SIZE = 1000;
	static int dum = 0;
	static int x = 0;
	static int s = 800;
	public static void main(String arg[]) {
		long t1, t2;

		int[][] a, b, c2,c ;
		int NTHREADS = Integer.parseInt(arg[0]);
		a = new int[SIZE][SIZE];
		b = new int[SIZE][SIZE];
		c2 = new int[SIZE][SIZE];
		c = new int[SIZE][SIZE];
		// INIT

		for (int i = 0; i < SIZE; i++)
			for (int j = 0; j < SIZE; j++) {
				a[i][j] = 1;
				b[i][j] = 2;
				dum++; // prevents parallelization
			}

//		for (int s = 200; s < SIZE; s += 100) {
		t1 = System.currentTimeMillis();
		/* par */
		for (int i = 0; i < s; i++)
			for (int j = 0; j < s; j++) {
				c[i][j] = 0;
				for (int k = 0; k < s; k++)
					c[i][j] += a[i][k] * b[k][j];
			}
		t2 = System.currentTimeMillis();

		double d = (t2 - t1) / 1000.0d;

		System.out.print(d + ",");
//			t1 = System.currentTimeMillis();
			/* par */
			for (int i = 0; i < s; i++)
				for (int j = 0; j < s; j++) {
					c2[i][j] = 0;
					for (int k = 0; k < s; k++)
						c2[i][j] += a[i][k] * b[k][j];
				}
			t2 = System.currentTimeMillis();
//
//
//			d = (t2 - t1) / 1000.0d;
//
//			System.out.println(s + " " + d);
			
			if (d<0)
			{
				t1 = System.currentTimeMillis();
				Thread[] threads = new Thread[NTHREADS];	
				for (int i = 0; i < NTHREADS ; i++) {
					final int lb = i * s/NTHREADS;
					final int ub = (i+1) * s/NTHREADS;
					threads[i] = new Thread(new Runnable() {

						public void run() {
							for (int i = lb; i < ub; i++)
								for (int j = 0; j < s; j++) {
									c2[i][j] = 0;
									for (int k = 0; k < s; k++)
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


				d = (t2 - t1) / 1000.0d;

				System.out.print(d);
				
			}

//		}
	}
}
