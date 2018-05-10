using UnityEngine;
using System;
using System.Runtime.InteropServices;

namespace VenusRendering
{
    public class Utility
    {
#if (UNITY_IPHONE) && !UNITY_EDITOR
	    [DllImport ("__Internal")]
#else
        [DllImport("VenusRendering")]
#endif
        public static extern int GetTestInteger();

#if (UNITY_IPHONE) && !UNITY_EDITOR
	    [DllImport ("__Internal")]
#else
        [DllImport("VenusRendering")]
#endif
        private static extern void InitLogFunc(IntPtr fp);

#if (UNITY_IPHONE) && !UNITY_EDITOR
	    [DllImport ("__Internal")]
#else
        [DllImport("VenusRendering")]
#endif
        private static extern void TermLogFunc();


        private static void Log(string str)
        {
            Debug.Log("VE_INFO: " + str);
        }

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate void LogDelegate(string str);

        //static MyDelegate callback_delegate = new MyDelegate(CallBackFunction);

        public static void Init()
        {
            InitLogFunc(Marshal.GetFunctionPointerForDelegate(new LogDelegate(Log)));
        }

        public static void Term()
        {
            TermLogFunc();
        }



    }

}
