#ifndef __FIREBASE_H__
#define __FIREBASE_H__

#include <cheerp/client.h>
namespace [[cheerp::genericjs]] client
{
	struct FirebaseConfig
	{
		void set_apiKey(const String&);
		void set_authDomain(const String&);
		void set_databaseURL(const String&);
		void set_projectId(const String&);
		void set_storageBucket(const String&);
		void set_messagingSenderId(const String&);
		void set_appId(const String&);
	};
	struct FirebaseReference;
	struct FirebaseDatabase
	{
		FirebaseReference* ref(const client::String&);
	};
	struct Firebase
	{
		void initializeApp(FirebaseConfig*);
		FirebaseDatabase* database();
	};
	extern Firebase& firebase;
	struct FirebaseReference
	{
		void on(const String&, EventListener*);
		void off(const String&, EventListener*);
		void off(const String&);
		Promise* once(const String&);
		void set(Object*);
		void update(Object*);
		void remove();
		FirebaseReference* push();
		FirebaseReference* child(const String&);
		FirebaseReference* orderByChild(const String&);
		FirebaseReference* startAt(double);
		FirebaseReference* endAt(double);
		String* get_key();
	};
	struct FirebaseSnapshot
	{
		template<class T>
		T* val();
		String* get_key();
		void forEach(client::EventListener* e);
		FirebaseReference* get_ref();
	};
}

#endif //__FIREBASE_H__