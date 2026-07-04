import iaik.security.provider.IAIK;

import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.Security;
import java.security.Signature;
import java.security.MessageDigest;
import java.security.interfaces.DSAPrivateKey;

import java.lang.management.ManagementFactory;
import java.lang.management.ThreadMXBean;
import java.math.BigInteger;
import java.util.Arrays;


public class DSA {

    BigInteger extractR(byte[] signature) throws Exception {
        int lengthR = signature[3];
        return new BigInteger(Arrays.copyOfRange(signature, 4, 4 + lengthR));
    }

    BigInteger extractS(byte[] signature) throws Exception {
        int lengthR = signature[3];
        int startS = 4 + lengthR;
        int lengthS = signature[startS + 1];
        return new BigInteger(Arrays.copyOfRange(signature, startS + 2, startS + 2 + lengthS));
    }

    BigInteger extractK(byte[] signature, BigInteger h, DSAPrivateKey priv)
        throws Exception {
        BigInteger x = priv.getX();
        BigInteger q = priv.getParams().getQ();
        BigInteger r = extractR(signature);
        BigInteger s = extractS(signature);
        BigInteger k = x.multiply(r).add(h).multiply(s.modInverse(q)).mod(q);
        return k;
    }

    public void signTiming(String algorithm, int iterations) throws Exception {
        ThreadMXBean bean = ManagementFactory.getThreadMXBean();
        if (!bean.isCurrentThreadCpuTimeSupported()) {
          System.out.println("getCurrentThreadCpuTime is not supported.");
          return;
        }

        KeyPairGenerator keyGenerator = KeyPairGenerator.getInstance(algorithm, "IAIK");
        KeyPair keyPair = keyGenerator.generateKeyPair();

        PrivateKey privateKey = keyPair.getPrivate();
        PublicKey publicKey = keyPair.getPublic();

        byte[] message = "Today is the day".getBytes();

        Signature dsa = Signature.getInstance(algorithm, "IAIK");
        byte[] digest = MessageDigest.getInstance("SHA1", "IAIK").digest(message);
        BigInteger h = new BigInteger(1, digest);

        dsa.initSign(privateKey);

        for (int i = 0; i < iterations; i++) {
            long start = bean.getCurrentThreadCpuTime();
            dsa.update(message);
            byte[] dsasig = dsa.sign();
            long time = bean.getCurrentThreadCpuTime() - start;

            BigInteger r = extractR(dsasig);
            BigInteger k = extractK(dsasig, h, (DSAPrivateKey) privateKey);
            String f = String.format("%d;%s;%s", time, r, k);
            System.out.println(f);
        }
    }

    public static void main(String arg[]) {
        Security.addProvider(new IAIK());
        DSA dsa = new DSA();        

        try {
            System.out.println("time;r;k");
            dsa.signTiming("SHA1withDSA", 100000);
        } catch (Exception e) {
            e.printStackTrace();
            throw new RuntimeException();
        }
    }
}
