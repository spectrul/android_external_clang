//===--- DeclSerialization.cpp - Serialization of Decls ---------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines methods that implement bitcode serialization for Decls.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "llvm/Bitcode/Serialize.h"
#include "llvm/Bitcode/Deserialize.h"

using llvm::Serializer;
using llvm::Deserializer;
using llvm::SerializedPtrID;

using namespace clang;

//===----------------------------------------------------------------------===//
// Decl Serialization: Dispatch code to handle specialized decl types.
//===----------------------------------------------------------------------===//

void Decl::Emit(Serializer& S) const {
  S.EmitInt(getKind());
  EmitImpl(S);
}

Decl* Decl::Create(Deserializer& D) {

  Kind k = static_cast<Kind>(D.ReadInt());
  
  switch (k) {
    default:
      assert (false && "Not implemented.");
      break;

    case BlockVar:
      return BlockVarDecl::CreateImpl(D);
      
    case Enum:
      return EnumDecl::CreateImpl(D);
      
    case EnumConstant:
      return EnumConstantDecl::CreateImpl(D);
      
    case Field:
      return FieldDecl::CreateImpl(D);
      
    case FileVar:
      return FileVarDecl::CreateImpl(D);
      
    case ParmVar:
      return ParmVarDecl::CreateImpl(D);
      
    case Function:
      return FunctionDecl::CreateImpl(D);
     
    case Union:
    case Struct:
      return RecordDecl::CreateImpl(k,D);
      
    case Typedef:
      return TypedefDecl::CreateImpl(D);
      
    case FileScopeAsm:
      return FileScopeAsmDecl::CreateImpl(D);
  }
}

//===----------------------------------------------------------------------===//
//      Common serialization logic for subclasses of Decl.
//===----------------------------------------------------------------------===//

void Decl::EmitInRec(Serializer& S) const {
  S.Emit(getLocation());                    // From Decl.
}

void Decl::ReadInRec(Deserializer& D) {
  Loc = SourceLocation::ReadVal(D);                 // From Decl.
}

//===----------------------------------------------------------------------===//
//      Common serialization logic for subclasses of NamedDecl.
//===----------------------------------------------------------------------===//

void NamedDecl::EmitInRec(Serializer& S) const {
  Decl::EmitInRec(S);
  S.EmitPtr(getIdentifier());               // From NamedDecl.
}

void NamedDecl::ReadInRec(Deserializer& D) {
  Decl::ReadInRec(D);
  D.ReadPtr(Identifier);                            // From NamedDecl.  
}

//===----------------------------------------------------------------------===//
//      Common serialization logic for subclasses of ScopedDecl.
//===----------------------------------------------------------------------===//

void ScopedDecl::EmitInRec(Serializer& S) const {
  NamedDecl::EmitInRec(S);
  S.EmitPtr(getNext());                     // From ScopedDecl.  
  S.EmitPtr(cast_or_null<Decl>(getContext()));  // From ScopedDecl.
}

void ScopedDecl::ReadInRec(Deserializer& D) {
  NamedDecl::ReadInRec(D);
  D.ReadPtr(Next);                                  // From ScopedDecl.
  Decl *TmpD;
  D.ReadPtr(TmpD);                                  // From ScopedDecl.
  CtxDecl = cast_or_null<ContextDecl>(TmpD);
}
    
  //===------------------------------------------------------------===//
  // NOTE: Not all subclasses of ScopedDecl will use the "OutRec"     //
  //   methods.  This is because owned pointers are usually "batched" //
  //   together for efficiency.                                       //
  //===------------------------------------------------------------===//

void ScopedDecl::EmitOutRec(Serializer& S) const {
  S.EmitOwnedPtr(getNextDeclarator());   // From ScopedDecl.
}

void ScopedDecl::ReadOutRec(Deserializer& D) {
  NextDeclarator = 
    cast_or_null<ScopedDecl>(D.ReadOwnedPtr<Decl>()); // From ScopedDecl.
}

//===----------------------------------------------------------------------===//
//      Common serialization logic for subclasses of ValueDecl.
//===----------------------------------------------------------------------===//

void ValueDecl::EmitInRec(Serializer& S) const {
  ScopedDecl::EmitInRec(S);
  S.Emit(getType());                        // From ValueDecl.
}

void ValueDecl::ReadInRec(Deserializer& D) {
  ScopedDecl::ReadInRec(D);
  DeclType = QualType::ReadVal(D);          // From ValueDecl.
}

//===----------------------------------------------------------------------===//
//      Common serialization logic for subclasses of VarDecl.
//===----------------------------------------------------------------------===//

void VarDecl::EmitInRec(Serializer& S) const {
  ValueDecl::EmitInRec(S);
  S.EmitInt(getStorageClass());             // From VarDecl.
}

void VarDecl::ReadInRec(Deserializer& D) {
  ValueDecl::ReadInRec(D);
  SClass = static_cast<StorageClass>(D.ReadInt());  // From VarDecl. 
}

    //===------------------------------------------------------------===//
    // NOTE: VarDecl has its own "OutRec" methods that doesn't use      //
    //  the one define in ScopedDecl.  This is to batch emit the        //
    //  owned pointers, which results in a smaller output.
    //===------------------------------------------------------------===//

void VarDecl::EmitOutRec(Serializer& S) const {
  // Emit these last because they will create records of their own.
  S.BatchEmitOwnedPtrs(getInit(),            // From VarDecl.
                       getNextDeclarator()); // From ScopedDecl.  
}

void VarDecl::ReadOutRec(Deserializer& D) {
  Decl* next_declarator;
  
  D.BatchReadOwnedPtrs(Init,                           // From VarDecl.
                       next_declarator);  // From ScopedDecl.
  
  setNextDeclarator(cast_or_null<ScopedDecl>(next_declarator));
}


void VarDecl::EmitImpl(Serializer& S) const {
  VarDecl::EmitInRec(S);
  VarDecl::EmitOutRec(S);
}

void VarDecl::ReadImpl(Deserializer& D) {
  ReadInRec(D);
  ReadOutRec(D);
}

//===----------------------------------------------------------------------===//
//      BlockVarDecl Serialization.
//===----------------------------------------------------------------------===//

BlockVarDecl* BlockVarDecl::CreateImpl(Deserializer& D) {  
  BlockVarDecl* decl = 
    new BlockVarDecl(0, SourceLocation(),NULL,QualType(),None,NULL);
 
  decl->VarDecl::ReadImpl(D);
  
  return decl;
}

//===----------------------------------------------------------------------===//
//      FileVarDecl Serialization.
//===----------------------------------------------------------------------===//

FileVarDecl* FileVarDecl::CreateImpl(Deserializer& D) {
  FileVarDecl* decl =
    new FileVarDecl(0, SourceLocation(),NULL,QualType(),None,NULL);
  
  decl->VarDecl::ReadImpl(D);

  return decl;
}

//===----------------------------------------------------------------------===//
//      ParmDecl Serialization.
//===----------------------------------------------------------------------===//

void ParmVarDecl::EmitImpl(llvm::Serializer& S) const {
  VarDecl::EmitImpl(S);
  S.EmitInt(getObjCDeclQualifier());        // From ParmVarDecl.
}

ParmVarDecl* ParmVarDecl::CreateImpl(Deserializer& D) {
  ParmVarDecl* decl =
    new ParmVarDecl(0, SourceLocation(),NULL,QualType(),None,NULL);
  
  decl->VarDecl::ReadImpl(D);
  decl->objcDeclQualifier = static_cast<ObjCDeclQualifier>(D.ReadInt());

  return decl;
}

//===----------------------------------------------------------------------===//
//      EnumDecl Serialization.
//===----------------------------------------------------------------------===//

void EnumDecl::EmitImpl(Serializer& S) const {
  ScopedDecl::EmitInRec(S);
  S.EmitBool(isDefinition());
  S.Emit(IntegerType);  
  S.BatchEmitOwnedPtrs(ElementList,getNextDeclarator());
}

EnumDecl* EnumDecl::CreateImpl(Deserializer& D) {
  EnumDecl* decl = new EnumDecl(0, SourceLocation(),NULL,NULL);
  
  decl->ScopedDecl::ReadInRec(D);
  decl->setDefinition(D.ReadBool());
  decl->IntegerType = QualType::ReadVal(D);
  
  Decl* next_declarator;
  Decl* Elist;
  
  D.BatchReadOwnedPtrs(Elist,next_declarator);
  
  decl->ElementList = cast_or_null<EnumConstantDecl>(Elist);
  decl->setNextDeclarator(cast_or_null<ScopedDecl>(next_declarator));
  
  return decl;
}

//===----------------------------------------------------------------------===//
//      EnumConstantDecl Serialization.
//===----------------------------------------------------------------------===//

void EnumConstantDecl::EmitImpl(Serializer& S) const {
  S.Emit(Val);
  ValueDecl::EmitInRec(S);
  S.BatchEmitOwnedPtrs(getNextDeclarator(),Init);
}
 
EnumConstantDecl* EnumConstantDecl::CreateImpl(Deserializer& D) {
  llvm::APSInt val(1);
  D.Read(val);
  
  EnumConstantDecl* decl = 
    new EnumConstantDecl(0, SourceLocation(),NULL,QualType(),NULL,
                         val,NULL);
  
  decl->ValueDecl::ReadInRec(D);
  
  Decl* next_declarator;
  
  D.BatchReadOwnedPtrs(next_declarator,decl->Init);
  
  decl->setNextDeclarator(cast_or_null<ScopedDecl>(next_declarator));

  return decl;    
}

//===----------------------------------------------------------------------===//
//      FieldDecl Serialization.
//===----------------------------------------------------------------------===//

void FieldDecl::EmitImpl(Serializer& S) const {
  S.Emit(getType());
  NamedDecl::EmitInRec(S);
  S.EmitOwnedPtr(BitWidth);  
}

FieldDecl* FieldDecl::CreateImpl(Deserializer& D) {
  FieldDecl* decl = new FieldDecl(0, SourceLocation(), NULL, QualType(), 0);
  decl->DeclType.ReadBackpatch(D);  
  decl->ReadInRec(D);
  decl->BitWidth = D.ReadOwnedPtr<Expr>();
  return decl;
}

//===----------------------------------------------------------------------===//
//      FunctionDecl Serialization.
//===----------------------------------------------------------------------===//

void FunctionDecl::EmitImpl(Serializer& S) const {
  S.EmitInt(SClass);           // From FunctionDecl.
  S.EmitBool(IsInline);        // From FunctionDecl.
  ValueDecl::EmitInRec(S);
  S.EmitPtr(DeclChain);
  
  // NOTE: We do not need to serialize out the number of parameters, because
  //  that is encoded in the type (accessed via getNumParams()).
  
  if (ParamInfo != NULL) {
    S.EmitBool(true);
    S.BatchEmitOwnedPtrs(getNumParams(),&ParamInfo[0], Body,
                         getNextDeclarator());
  }
  else {
    S.EmitBool(false);
    S.BatchEmitOwnedPtrs(Body,getNextDeclarator());  
  }
}

FunctionDecl* FunctionDecl::CreateImpl(Deserializer& D) {
  StorageClass SClass = static_cast<StorageClass>(D.ReadInt());
  bool IsInline = D.ReadBool();
  
  FunctionDecl* decl =
    new FunctionDecl(0, SourceLocation(),NULL,QualType(),SClass, IsInline, 0);
  
  decl->ValueDecl::ReadInRec(D);
  D.ReadPtr(decl->DeclChain);

  Decl* next_declarator;
  
  bool hasParamDecls = D.ReadBool();
    
  decl->ParamInfo = hasParamDecls
                  ? new ParmVarDecl*[decl->getNumParams()] 
                  : NULL;  
  
  if (hasParamDecls)
    D.BatchReadOwnedPtrs(decl->getNumParams(),
                         reinterpret_cast<Decl**>(&decl->ParamInfo[0]),
                         decl->Body, next_declarator);
  else
    D.BatchReadOwnedPtrs(decl->Body, next_declarator);
  
  decl->setNextDeclarator(cast_or_null<ScopedDecl>(next_declarator));
  
  return decl;
}

//===----------------------------------------------------------------------===//
//      RecordDecl Serialization.
//===----------------------------------------------------------------------===//

void RecordDecl::EmitImpl(Serializer& S) const {
  ScopedDecl::EmitInRec(S);
  S.EmitBool(isDefinition());
  S.EmitBool(hasFlexibleArrayMember());
  S.EmitSInt(getNumMembers());
  if (getNumMembers() > 0) {
    assert (Members);
    S.BatchEmitOwnedPtrs((unsigned) getNumMembers(),
                         (Decl**) &Members[0],getNextDeclarator());
  }
  else
    ScopedDecl::EmitOutRec(S);
}

RecordDecl* RecordDecl::CreateImpl(Decl::Kind DK, Deserializer& D) {
  RecordDecl* decl = new RecordDecl(DK,0,SourceLocation(),NULL,NULL);
    
  decl->ScopedDecl::ReadInRec(D);
  decl->setDefinition(D.ReadBool());
  decl->setHasFlexibleArrayMember(D.ReadBool());
  decl->NumMembers = D.ReadSInt();
  
  if (decl->getNumMembers() > 0) {
    Decl* next_declarator;
    decl->Members = new FieldDecl*[(unsigned) decl->getNumMembers()];
                              
    D.BatchReadOwnedPtrs((unsigned) decl->getNumMembers(),
                         (Decl**) &decl->Members[0],
                         next_declarator);
    
    decl->setNextDeclarator(cast_or_null<ScopedDecl>(next_declarator));                             
  }
  else
    decl->ScopedDecl::ReadOutRec(D);
  
  return decl;
}

//===----------------------------------------------------------------------===//
//      TypedefDecl Serialization.
//===----------------------------------------------------------------------===//

void TypedefDecl::EmitImpl(Serializer& S) const {
  S.Emit(UnderlyingType);
  ScopedDecl::EmitInRec(S);
  ScopedDecl::EmitOutRec(S);
}

TypedefDecl* TypedefDecl::CreateImpl(Deserializer& D) {
  QualType T = QualType::ReadVal(D);
  
  TypedefDecl* decl = new TypedefDecl(0, SourceLocation(),NULL,T,NULL);
  
  decl->ScopedDecl::ReadInRec(D);
  decl->ScopedDecl::ReadOutRec(D);

  return decl;
}

//===----------------------------------------------------------------------===//
//      LinkageSpec Serialization.
//===----------------------------------------------------------------------===//

void LinkageSpecDecl::EmitInRec(Serializer& S) const {
  Decl::EmitInRec(S);
  S.EmitInt(getLanguage());
  S.EmitPtr(D);
}

void LinkageSpecDecl::ReadInRec(Deserializer& D) {
  Decl::ReadInRec(D);
  Language = static_cast<LanguageIDs>(D.ReadInt());
  D.ReadPtr(this->D);
}

//===----------------------------------------------------------------------===//
//      FileScopeAsm Serialization.
//===----------------------------------------------------------------------===//

void FileScopeAsmDecl::EmitImpl(llvm::Serializer& S) const
{
  Decl::EmitInRec(S);
  S.EmitOwnedPtr(AsmString);
}

FileScopeAsmDecl* FileScopeAsmDecl::CreateImpl(Deserializer& D) { 
  FileScopeAsmDecl* decl = new FileScopeAsmDecl(SourceLocation(), 0);

  decl->Decl::ReadInRec(D);
  decl->AsmString = cast<StringLiteral>(D.ReadOwnedPtr<Expr>());
//  D.ReadOwnedPtr(D.ReadOwnedPtr<StringLiteral>())<#T * * Ptr#>, <#bool AutoRegister#>)(decl->AsmString);
  
  return decl;
}
