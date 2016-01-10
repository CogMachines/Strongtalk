#include "gtest/gtest.h"

#include "incls/_precompiled.incl"
#include "incls/_node.cpp.incl"

#include "testNotifier.hpp"


class UncommonSendNodeTests : public ::testing::Test {
protected:
  HeapResourceMark *mark;
  UncommonSendNode *node;
  GrowableArray<PReg *> *exprStack;
  Notifier *saved;
  TestNotifier *notifier;
  InlinedScope *topScope;
  BB *bb;
void checkSingleUse(int index, Node *expected) {
  DUInfo *info = bb->duInfo.info->at(index);
  ASSERT_EQ(1, info->uses.length());
  Use *use = info->uses.at(0);
  ASSERT_TRUE(expected == use->node);
}
protected:
virtual void SetUp() {
  mark = new HeapResourceMark();
  saved = Notifier::current;
  notifier = new TestNotifier;
  Notifier::current = notifier;

  LookupKey key(klassOop(Universe::find_global("Object")), oopFactory::new_symbol("="));
  LookupResult result = lookupCache::lookup(&key);

  theCompiler = new Compiler(&key, result.method());
  topScope = theCompiler->topScope;
  theCompiler->enterScope(topScope);
  topScope->createTemporaries(1);

  NodeFactory::cumulCost = 0;
  exprStack = new GrowableArray<PReg*>(10);
}
virtual void TearDown() {
  theCompiler = NULL;
  node = NULL;
  Notifier::current = saved;
  notifier = NULL;
  delete mark;
  mark = NULL;
}
};

TEST_F(UncommonSendNodeTests, construction) {
  node = NodeFactory::new_UncommonSendNode(exprStack, 0, 0);

  ASSERT_EQ(4, NodeFactory::cumulCost);
  ASSERT_TRUE(node->isUncommonSendNode());
}

TEST_F(UncommonSendNodeTests, cloneShouldCopyContents) {
  node = NodeFactory::new_UncommonSendNode(exprStack, 1, 2);

  Node* clone = node->clone(NULL, NULL);

  ASSERT_TRUE(clone->isUncommonSendNode());
  UncommonSendNode* clonedNode = ((UncommonSendNode*)clone);

  ASSERT_EQ(1, clonedNode->bci());
  ASSERT_EQ(2, clonedNode->args());
  ASSERT_TRUE(clonedNode->expressionStack() != node->expressionStack());
}

TEST_F(UncommonSendNodeTests, verifyShouldCallErrorWhenNotLastInBB) {
  node = NodeFactory::new_UncommonSendNode(exprStack, 1, 0);
  Node* nop = NodeFactory::new_NopNode();
  Node* comment = NodeFactory::new_CommentNode("test");
  nop->append(node);
  node->append(comment);

  BB* bb = new BB(nop, comment, 0);
  node->setBB(bb);

  node->verify();

  ASSERT_EQ(1, notifier->errorCount());
}

TEST_F(UncommonSendNodeTests, verifyShouldCallErrorWhenMoreArgsThanExpressions) {
  node = NodeFactory::new_UncommonSendNode(exprStack, 1, 1);
  Node* nop = NodeFactory::new_NopNode();
  nop->append(node);

  char buffer[2048];
  sprintf(buffer, "Too few expressions on stack for 0x%x: required %d, but got %d", node, 1, 0);
  BB* bb = new BB(nop, node, 0);
  node->setBB(bb);

  node->verify();

  ASSERT_EQ(1, notifier->errorCount());
  ASSERT_EQ(0, strcmp(buffer, notifier->errorAt(0)));
}

TEST_F(UncommonSendNodeTests, print_stringShouldReturnFormattedString) {
  node = NodeFactory::new_UncommonSendNode(exprStack, 1, 1);

  char buffer[2048];
  node->print_string(buffer, false);

  ASSERT_EQ(0, strncmp("UncommonSend(1 arg)", buffer, 19)) << buffer;
}

TEST_F(UncommonSendNodeTests, print_stringShouldReturnFormattedStringWithAddress) {
  node = NodeFactory::new_UncommonSendNode(exprStack, 1, 1);

  char expected[100];
  sprintf(expected, "UncommonSend(1 arg)                     ((UncommonSendNode*)%#lx)", node);
  char buffer[100];
  node->print_string(buffer);

  ASSERT_EQ(0, strcmp(expected, buffer)) << buffer;
}

TEST_F(UncommonSendNodeTests, makeUsesShouldAddUseForOneArgument) {
  PReg* expr = new PReg(topScope);
  exprStack->append(expr);

  Node* nop = NodeFactory::new_NopNode();
  node = NodeFactory::new_UncommonSendNode(exprStack, 1, 1);
  nop->append(node);

  bbIterator->pregTable = new GrowableArray<PReg*>;

  bb = new BB(nop, node, 2);
  node->setBB(bb);

  ASSERT_TRUE(bb->duInfo.info == NULL);
  bb->makeUses();
  ASSERT_EQ(1, bb->duInfo.info->length());
  checkSingleUse(0, node);
}


TEST_F(UncommonSendNodeTests, makeUsesShouldAddUseForTwoArguments) {
  PReg* arg1 = new PReg(topScope);
  PReg* arg2 = new PReg(topScope);
  exprStack->append(arg1);
  exprStack->append(arg2);

  Node* nop = NodeFactory::new_NopNode();
  node = NodeFactory::new_UncommonSendNode(exprStack, 1, 2);
  nop->append(node);

  bbIterator->pregTable = new GrowableArray<PReg*>;

  bb = new BB(nop, node, 2);
  node->setBB(bb);

  ASSERT_TRUE(bb->duInfo.info == NULL);
  bb->makeUses();
  ASSERT_EQ(2, bb->duInfo.info->length());
  checkSingleUse(0, node);
  checkSingleUse(1, node);
}