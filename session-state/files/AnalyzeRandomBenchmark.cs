using System;
using System.Collections.Generic;
using System.IO;
using System.Numerics;

static class A
{
 const uint TableSize=1000000;
 const double Tol=1e-6;
 static ulong Mix64(ulong v){unchecked{v+=(ulong)0x9E3779B97F4A7C15UL;v=(v^(v>>30))*0xBF58476D1CE4E5B9UL;v=(v^(v>>27))*0x94D049BB133111EBUL;return v^(v>>31);}}
 static long Quantize(double c){return (long)Math.Round(c/Tol, MidpointRounding.AwayFromZero);}
 static ulong HashPoint(double x,double y,double z){ulong h=Mix64(unchecked((ulong)Quantize(x)));h=Mix64(h^unchecked((ulong)Quantize(y)));h=Mix64(h^unchecked((ulong)Quantize(z)));return h;}
 static uint HashTriangle(double[] t,bool oi){ulong[] h=new ulong[3];for(int i=0;i<3;i++)h[i]=HashPoint(t[i*3],t[i*3+1],t[i*3+2]);if(oi)Array.Sort(h);ulong k=Mix64(h[0]);k=Mix64(k^h[1]);k=Mix64(k^h[2]); ulong p=(ulong)((BigInteger)k*TableSize>>64);return (uint)p;}

 public static void Main(){
  var triangles=new List<double[]>();var verts=new List<double[]>();foreach(var line in File.ReadLines("GeometricHashTable/wavy_surface_100000.stl")){var p=line.Trim().Split((char[])null,StringSplitOptions.RemoveEmptyEntries);if(p.Length>0&&p[0]=="vertex"){} else continue;var v=new[]{double.Parse(p[1],System.Globalization.CultureInfo.InvariantCulture),double.Parse(p[2],System.Globalization.CultureInfo.InvariantCulture),double.Parse(p[3],System.Globalization.CultureInfo.InvariantCulture)};verts.Add(v);if(verts.Count==3){var t=new double[9];var sorted=new List<double[]>(verts);sorted.Sort((a,b)=>a[0]!=b[0]?a[0].CompareTo(b[0]):a[1]!=b[1]?a[1].CompareTo(b[1]):a[2].CompareTo(b[2]);for(int i=0;i<3;i++)Array.Copy(sorted[i],0,t,i*3,3);triangles.Add(t);verts.Clear();if(triangles.Count==16384)break;}}
  var rt=new Dictionary<uint,double[]>();var rf=new Dictionary<uint,double[]>();foreach(var t in triangles){uint kt=HashTriangle(t,true),kf=HashTriangle(t,false);if(!rt.ContainsKey(kt))rt[kt]=t;if(!rf.ContainsKey(kf))rf[kf]=t;}
  foreach(int count in new[]{64,1024,16384}){int mismatches=0,matchingTrue=0,matchingFalse=0;for(int i=0;i<count;i++){var t=triangles[i];var bt=rt.ContainsKey(HashTriangle(t,true));var bf=rf.ContainsKey(HashTriangle(t,false)); // equality not checked; isolate hash-only mismatch
    matchingTrue+=bt?1:0;matchingFalse+=bf?1:0;if(bt!=bf)mismatches++;}Console.WriteLine("count="+count+" queryBucketsTrue="+matchingTrue+" queryBucketsFalse="+matchingFalse+" roughMismatch="+mismatches);}
 }
}
